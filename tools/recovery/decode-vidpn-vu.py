#!/usr/bin/env python3
"""8715 VU integer framing only. A complete stream never proves current boot."""
import argparse
import hashlib
import json
from pathlib import Path
import re

LENGTHS = {0x3d0: 32, 0x3d1: 16, 0x3d2: 36, 0x3d3: 4, 0x3df: 4}
LINE = re.compile(rb'VU ([0-9A-F]{8})(?: ([0-9A-F]{8}))(?: ([0-9A-F]{8}))(?: ([0-9A-F]{8}))((?: [0-9A-F]{8}){1,6})\r?\n')
# BOOT_REASON is an actual boot boundary used by the existing host observer.
# Other/new boot markers require an externally delimited byte window; absence
# of this marker is NEVER a current-boot proof.
BOOT = re.compile(rb'\bBOOT_REASON\b')
MAX_INPUT = 16 * 1024 * 1024


class Rejected(ValueError):
    pass


def require(condition, reason):
    if not condition:
        raise Rejected(reason)


def digest(words):
    return b''.join(w.to_bytes(4, 'little') for w in words).hex()


def decode(raw, expected_run, expected_pid):
    require(len(raw) <= MAX_INPUT, 'input_window_oversize')
    require(0 <= expected_run <= 0xffffffff and 0 < expected_pid <= 0xffffffff, 'expected_identity')
    packets, frame_bytes, wire_lines = [], bytearray(), 0
    active_event, active, started, offset = None, [], False, 0
    first_offset = last_offset = None
    for piece in raw.split(b'\n'):
        # Preserve exact LF/CRLF framing and reject a truncated final VU line.
        end = offset + len(piece)
        line = piece + (b'\n' if end < len(raw) else b'')
        if started:
            require(not BOOT.search(line), 'boot_boundary_after_vu_begin')
        candidate = line.startswith(b'VU') or b'VU ' in line
        if candidate:
            require(len(line) <= 94, 'vu_line_oversize')
            match = LINE.fullmatch(line)
            require(match is not None, 'malformed_or_partial_vu_line')
            started = True
            if first_offset is None:
                first_offset = offset
            last_offset = offset + len(line)
            wire_lines += 1
            frame_bytes.extend(line)
            require(wire_lines <= 100 and len(frame_bytes) <= 9400, 'vu_stream_oversize')
            event, ordinal, position, count = (int(match[i], 16) for i in range(1, 5))
            values = [int(w, 16) for w in match[5].split()]
            require(event in LENGTHS, 'unknown_event')
            require(len(packets) < 18, 'packet_count_oversize')
            require(not packets or packets[-1][0] not in (0x3d3, 0x3df), 'vu_after_terminal_packet')
            require(ordinal == len(packets), 'missing_duplicate_or_reordered_packet')
            if active_event is None:
                active_event = event
            require(event == active_event and position == len(active), 'mixed_duplicate_or_missing_chunk')
            remaining = LENGTHS[event] - len(active)
            require(count == len(values) == min(6, remaining) and count > 0, 'chunk_count')
            active.extend(values)
            if len(active) == LENGTHS[event]:
                packets.append((event, active))
                active_event, active = None, []
        offset += len(line)
    require(started, 'missing_vu')
    require(active_event is None, 'partial_packet')
    if len(packets) == 1 and packets[0][0] == 0x3df:
        error = packets[0][1]
        require(error[0] == 1 and 1 <= error[1] <= 8 and error[2:] == [0, 0], 'error_packet_shape')
        raise Rejected('helper_reported_error_' + str(error[1]))
    require(len(packets) >= 3 and packets[0][0] == 0x3d0 and packets[1][0] == 0x3d1
            and packets[-1][0] == 0x3d3 and all(e == 0x3d2 for e, _ in packets[2:-1]), 'packet_order_or_missing_END')
    binding, summary, end = packets[0][1], packets[1][1], packets[-1][1]
    require(binding[0] == 1 and binding[1:3] == [expected_run, expected_pid], 'run_pid_mismatch')
    require(binding[3] == 0 and any(binding[6:8]) and binding[21] != 0, 'epoch_or_current_boot_claim')
    require(binding[22] == int(any(binding[8:13])) and binding[23] == int(any(binding[13:21]))
            and binding[24] in (0, 1) and (not binding[24] or binding[23])
            and not any(binding[25:]), 'binding_flags_or_reserved')
    require(summary[:2] == [1, 432] and 1 <= summary[2] <= 44
            and summary[3] + summary[4] == summary[2] - 1 and summary[5] <= summary[3]
            and summary[6] <= summary[3] and summary[7] in (0, 1) and summary[8] <= 127
            and summary[9] in (0, 1) and summary[11] == 1 and not any(summary[12:]), 'summary_shape')
    records, previous, incomplete, negative = [], 0, 0, 0
    for _, record in packets[2:-1]:
        index, slot, complete, status = record[:4]
        require(1 <= index <= 43 and (index == 43 or (index-1) % 6 in (4, 5))
                and index > previous and complete in (0, 1), 'record_index_duplicate_or_order')
        category = 'U' if index == 43 else ('F' if (index-1) % 6 == 4 else 'L')
        kind = 1 if index == 43 else 1 + (index-1) // 6
        require(category != 'F' or (complete == 1 and status >= 0x80000000), 'failure_record_shape')
        require(category != 'U' or (complete == 1 and status == 0 and record[15] == 0), 'semantic_FALSE_shape')
        records.append(dict(index=index, kind=kind, category=category, slot=slot, completed=complete,
                            status=status, numeric_words=record[4:], raw_epoch_match_declared=True))
        previous = index
        incomplete += int(not complete)
        negative += int(bool(complete) and status >= 0x80000000)
    n = len(records)
    first_count = summary[3] - n
    kinds = summary[8].bit_count()
    require(n <= 15 and kinds <= first_count <= 4*kinds, 'first_record_count_mask')
    require(incomplete <= summary[5] <= incomplete + first_count
            and negative <= summary[6] <= negative + first_count
            and summary[7] == int(any(rec['index'] == 43 for rec in records)), 'summary_record_contradiction')
    require(end == [1, n, len(packets), 52 + 36*n] and end[3] <= 592, 'END_counts')
    return dict(schema='GE8300_VIDPN_VU_HOST_V1', transport_complete=True,
                current_boot_unproven=True, callback_absence_proven=False, hardware_success=False,
                source_proven=False, deployed_SYS_proven=False, host_seal_binding_proven=False,
                PB_run=binding[1], PB_PID=binding[2], raw_epoch_sequence=binding[21],
                raw_epoch_FILETIME=binding[4] | binding[5] << 32,
                raw_epoch_interrupttime=binding[6] | binding[7] << 32,
                declared_writer_source_sha1=digest(binding[8:13]) if binding[22] else None,
                declared_SYS_sha256=digest(binding[13:21]) if binding[23] else None,
                caller_SYS_verified_flag=binding[24], summary_words=summary, records=records,
                wire_lines=wire_lines, wire_bytes=len(frame_bytes), first_VU_byte=first_offset,
                after_last_VU_byte=last_offset, input_sha256=hashlib.sha256(raw).hexdigest(),
                VU_sha256=hashlib.sha256(frame_bytes).hexdigest())


def u32(value):
    number = int(value, 0)
    if not 0 <= number <= 0xffffffff:
        raise argparse.ArgumentTypeError('u32 required')
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('uart', type=Path)
    parser.add_argument('--expected-run', type=u32, required=True)
    parser.add_argument('--expected-pid', type=u32, required=True)
    parser.add_argument('--start-byte', type=int, default=0)
    parser.add_argument('--end-byte', type=int)
    args = parser.parse_args()
    try:
        size = args.uart.stat().st_size
        end = size if args.end_byte is None else args.end_byte
        require(0 <= args.start_byte <= end <= size and end-args.start_byte <= MAX_INPUT, 'invalid_input_window')
        with args.uart.open('rb') as stream:
            stream.seek(args.start_byte)
            raw = stream.read(end-args.start_byte)
        result = decode(raw, args.expected_run, args.expected_pid)
        result.update(window_start_byte=args.start_byte, window_end_byte=end)
        print(json.dumps(result, indent=2))
    except (Rejected, OSError) as error:
        print(json.dumps(dict(schema='GE8300_VIDPN_VU_HOST_V1', transport_complete=False,
                              current_boot_unproven=True, hardware_success=False, error=str(error))))
        return 2
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
