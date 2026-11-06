#!/usr/bin/env python3
"""Decode CRC-framed A04CP1 configured CPU register observations."""
import argparse
import json
import re
import zlib

PATTERN = re.compile(rb'(?<![A-Za-z0-9])A04CP1(?: [0-9A-F]{8}){8}(?=\r?\n|$)')

def decode(data):
    runs = {}
    invalid = 0
    for match in PATTERN.finditer(data):
        fields = match.group().split(b' ')
        if zlib.crc32(b' '.join(fields[:-1])) != int(fields[-1], 16):
            invalid += 1
            continue
        run, seq, kind, *values = (int(x, 16) for x in fields[1:-1])
        samples = runs.setdefault(f'{run:08X}', {})
        slot = samples.setdefault(str(seq), {})
        name = f'{kind:08X}'
        if name in slot and slot[name] != values:
            slot['contradictory'] = True
        else:
            slot[name] = values
    return {'runs': runs, 'invalid_crc': invalid,
            'interpretation': 'Configured PLL and VPROC selector only; not measured frequency, voltage, temperature, or a stability verdict.'}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('log');p.add_argument('--start-offset', type=int, required=True)
    p.add_argument('--end-offset', type=int)
    args = p.parse_args()
    with open(args.log, 'rb') as f:
        f.seek(args.start_offset)
        data = f.read() if args.end_offset is None else f.read(args.end_offset-args.start_offset)
    print(json.dumps(decode(data), indent=2))
if __name__ == '__main__':
    main()
