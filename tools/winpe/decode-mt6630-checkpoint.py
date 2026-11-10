#!/usr/bin/env python3
"""Decode only the fixed binary checkpoint; never mounts/modifies a hive."""
import argparse
import json
from pathlib import Path
import struct

HEADER = struct.Struct('<4I4Q10I')
EVENT = struct.Struct('<6I')
CAPACITY = 64
STAGES = {
    100:'CONNECT_CONTEXT',101:'CONNECT_OID',102:'CONNECT_PRIOR_REJECTION',103:'CONNECT_GATE',104:'CONNECT_POLICY',105:'CONNECT_CANDIDATE',106:'CONNECT_WAKE',107:'CONNECT_SELECT',108:'CONNECT_CHANNEL',109:'CONNECT_AUTH_TX',110:'CONNECT_AUTH_RX',111:'CONNECT_ASSOC_TX',112:'CONNECT_ASSOC_RX',113:'CONNECT_STA',114:'CONNECT_WAIT',115:'CONNECT_OID_COMPLETE',116:'CONNECT_ASSOC_COMPLETE',117:'CONNECT_CONNECT_COMPLETE',118:'CONNECT_RX_DECODE',119:'CONNECT_EARLY_RX',120:'CONNECT_EAPOL_RX',121:'CONNECT_EAPOL_TX',122:'CONNECT_TX_DONE',123:'CONNECT_KEY_OID',124:'CONNECT_KEY_CID7',125:'CONNECT_KEY_EVENT',126:'CONNECT_PORT',127:'CONNECT_END',128:'RESET_BOUNDARY',
    89:'RX_RESULT',90:'RX_DESCRIPTOR',91:'AP_REJECT',92:'AP_REJECT_DESCRIPTOR',88:'LAST_OID',80:'RUNTIME_STATE',81:'HOST_STATE',82:'CREDITS',83:'WHISR',84:'WASR',85:'LAST_COMMAND',86:'SCAN_STATE',87:'RUNTIME_RESULT',
    1: 'REGISTER_DRIVER', 2: 'INIT_ENTRY', 3: 'CONTEXT_ALLOCATION',
    4: 'REGISTRATION_ATTRIBUTES', 5: 'RX_POOL', 6: 'RX_SLOTS',
    7: 'CONFIGURATION_OPEN', 8: 'RADIO_SETTING', 9: 'PARENT_QUERY',
    10: 'RESOURCES', 11: 'SESSION_START', 12: 'IRQ_CONNECT', 13: 'TC1_ENABLE',
    14: 'STATION_INIT', 15: 'PS_PROFILE', 16: 'GENERAL_ATTRIBUTES',
    17: 'NATIVE_ATTRIBUTE_ALLOCATION', 18: 'NATIVE_ATTRIBUTES',
    19: 'INIT_RETURN_PREPARED', 20: 'CLEANUP_RETURN_OBSERVED',
    21: 'DRIVER_RETURN_PREPARED', 22: 'DISABLED_IDENTITY_READ',
    23: 'REGULATORY_DECODE', 40: 'SESSION_ACQUIRE', 41: 'SESSION_MAP',
    42: 'SESSION_ACTIVATE', 43: 'SESSION_DIAGNOSTIC', 44: 'SESSION_IRQ_CALLBACK',
    45: 'FW_READ_WMT', 46: 'FW_READ_WLAN', 47: 'FW_HASH_WMT', 48: 'FW_HASH_WLAN',
    49: 'DIAGNOSTIC_BOARD_INIT', 50: 'DIAGNOSTIC_CONTROL_OPEN',
    51: 'FW_SELECT_WMT', 52: 'FW_SELECT_WLAN', 53: 'FW_BOOT_WMT',
    54: 'FW_BOOT_WLAN', 55: 'DIAGNOSTIC_CALIBRATION',
    56: 'CALIBRATION_ALLOCATION', 57: 'CAL_SYSTEM_EXTENTS', 59: 'CAL_DISK_MATCH',
    65: 'CAL_NVRAM_READ', 66: 'CAL_REGULATORY_DECODE', 67: 'CAL_RUNTIME_BEGIN',
    68: 'CAL_MANUFACTURE_SUBMIT', 69: 'CAL_RECORD_VERSIONS',
}
KINDS = {0: 'UNOBSERVED', 1: 'NDIS_STATUS', 2: 'NTSTATUS', 3: 'BACKEND_INT', 4: 'OBSERVATION'}
PHASES = {1: 'BEGIN', 2: 'RESULT', 3: 'RETURN_PREPARED'}


def decode(data, after_generation=None):
    if len(data) != HEADER.size + CAPACITY * EVENT.size:
        raise ValueError('Wrong fixed record length')
    h = HEADER.unpack_from(data)
    magic, version, size, channel, generation, parent, ticks, system_time = h[:8]
    count, overflow, diag_known, diag_status, flush_calls, flush_known, flush_status, tail_known, prepared, build = h[8:]
    if magic != 0x50433441 or version != 1 or size != len(data) or channel not in (0, 1, 2, 3):
        raise ValueError('Unknown checkpoint schema/channel')
    if not generation or count > CAPACITY or tail_known != 0 or flush_calls > 10:
        raise ValueError('Invalid checkpoint bounds or unsupported tail-flush claim')
    events = []
    for index in range(count):
        stage, phase, kind, observed, raw, detail = EVENT.unpack_from(data, HEADER.size + index * EVENT.size)
        if stage not in STAGES or phase not in PHASES or kind not in KINDS or observed not in (0, 1):
            raise ValueError('Unknown event schema')
        if phase == 1 and observed:
            raise ValueError('A BEGIN cannot assert an observed result')
        events.append(dict(stage=STAGES[stage], phase=PHASES[phase], kind=KINDS[kind],
                           observed=bool(observed), raw_status=f'0x{raw:08x}' if observed and kind else None,
                           detail=detail))
        if stage == 128:
            boundary = detail & 255
            if phase != 2 or kind != 3 or not observed or boundary not in (1, 2, 3) or detail & ~0x1fff:
                raise ValueError('Unknown RESET boundary event')
            broker = bool(detail & (1 << 12))
            events[-1]['reset_boundary'] = dict(
                boundary={1: 'radio_reset', 2: 'default_mib_profile', 3: 'final_idle'}[boundary],
                backend_return=raw if raw < 2**31 else raw - 2**32,
                entry_default_mib=bool(detail & (1 << 8)), entry_power=bool(detail & (1 << 9)),
                entry_broker_compiled=broker,
                entry_hci_open=bool(detail & (1 << 10)) if broker else None,
                entry_sco_held=bool(detail & (1 << 11)) if broker else None)
        if observed and stage == 69:
            events[-1]['record_versions'] = dict(part1_own=raw & 65535, part2_own=raw >> 16,
                                                 part1_peer=detail & 65535, part2_peer=detail >> 16)
        if observed and stage == 66 and build >= 1008:
            events[-1]['regulatory_inspection'] = dict(reason=detail & 255, selection=(detail >> 8) & 15,
                                                       country_class=(detail >> 12) & 15,
                                                       map=(detail >> 16) & 255, index=detail >> 24)
        if observed and stage in (67, 68) and build >= 1009:
            receipt = detail if stage == 67 else detail >> 8
            mask = (receipt >> 16) & 7
            events[-1]['runtime_boundary'] = dict(phase=receipt & 255,
                release_register_reads=(receipt >> 8) & 255,
                wcir_observed=bool(mask & 1), whisr_observed=bool(mask & 2),
                tx_done_observed=bool(mask & 4), completed=(receipt & 255) == 5)
            if stage == 68:
                events[-1]['last_manufacture_cid'] = detail & 255
    return dict(schema=version, driver_build=build, channel=('DriverEntry', 'Initialize', 'Scan', 'RuntimeFailure')[channel],
                generation=generation, parent_generation=parent, interrupt_time=ticks,
                system_time=system_time,
                freshness=('unknown' if after_generation is None else
                           'newer_than_baseline' if generation > after_generation else 'stale'),
                overflow=bool(overflow), diagnostic_error=f'0x{diag_status:08x}' if diag_known else None,
                recorded_flush_calls=flush_calls,
                last_observed_flush=f'0x{flush_status:08x}' if flush_known else None,
                tail_flush_result='unobserved', durable_success=False,
                return_prepared=bool(prepared), ndis_handoff_observed=False,
                connection_trace=(channel==3 and build>=1029),
                connection_binding=('initialize_parent_generation; context1=preconnect/context2=CONNECT_REQUEST' if channel==3 and build>=1029 else 'legacy'),
                history_truncated=bool(overflow),
                events=events, unobserved_stages=[v for k, v in STAGES.items()
                                                if not any(e['stage'] == v and e['observed'] for e in events)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('record', type=Path, help='Raw REG_BINARY exported by the main operator')
    parser.add_argument('--after-generation', type=int, help='Pre-test native-replayed baseline counter')
    args = parser.parse_args()
    if args.after_generation is not None and not 0 <= args.after_generation < 2**64:
        parser.error('Invalid baseline generation')
    print(json.dumps(decode(args.record.read_bytes(), args.after_generation), indent=2))


if __name__ == '__main__':
    main()
