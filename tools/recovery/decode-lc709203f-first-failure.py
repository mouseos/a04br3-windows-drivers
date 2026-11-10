#!/usr/bin/env python3
"""既single-load readerで取得した固定2値だけをdecode。hive/端末操作なし。"""
import argparse
import json
import struct

MAGIC = 0x3146474C
STAGES = {1: 'DriverEntry', 2: 'DeviceAdd', 3: 'PrepareHardware',
          4: 'D0Entry', 5: 'SelfManagedIoInit'}


def words(text, count):
    if len(text) != count * 8:
        raise ValueError('fixed REG_BINARY size mismatch')
    return struct.unpack('<' + 'I' * count, bytes.fromhex(text))


def decode(epoch_hex, failure_hex=None):
    e = words(epoch_hex, 4)
    if e[:2] != (MAGIC, 1):
        raise ValueError('unknown load epoch schema')
    result = dict(load_FILETIME=e[2] | e[3] << 32,
                  hardware_success_claimed=False, failure_observed=False)
    if failure_hex is None:
        return dict(result, observation='failure-record-absent-not-proof-of-success')
    r = words(failure_hex, 24)
    if (r[:3] != (MAGIC, 1, 96) or r[3] not in STAGES or
            not r[4] & 0x80000000 or r[15] & ~31 or any(r[22:])):
        raise ValueError('unknown/invalid failure record')
    current = r[5:7] == e[2:4]
    result.update(current_load=current, failure_observed=current,
                  observation='current-load-first-failure' if current else 'stale-other-driver-load',
                  callback=STAGES[r[3]], NTSTATUS=f'{r[4]:08X}',
                  failure_load_FILETIME=r[5] | r[6] << 32,
                  failure_FILETIME=r[7] | r[8] << 32,
                  gauge_faulted=r[9], adapter_faulted=r[10],
                  fault_reason=r[11], fault_command=r[12],
                  fault_status=r[13], fault_fifo=r[14],
                  admission_valid_mask=r[15],
                  admission={name: r[16+i] if r[15] & (1 << i) else None
                             for i, name in enumerate(('START24', 'TIMING20', 'DMA48', 'IO_CONFIG40', 'HS60'))},
                  previous_power_state=r[21], same_boot_join_required=True)
    return result


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--epoch-hex', required=True)
    p.add_argument('--failure-hex')
    a = p.parse_args()
    print(json.dumps(decode(a.epoch_hex, a.failure_hex), indent=2))
