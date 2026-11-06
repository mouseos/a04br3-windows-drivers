#!/usr/bin/env python3
"""CRC付きGPUコピー診断を検証する。欠落・矛盾・混線は成功としない。"""
import argparse
import json
import re
import zlib
from pathlib import Path

PAT = re.compile(rb'^A04CP1 (?:[0-9A-F]{8} ){7}[0-9A-F]{8} [0-9A-F]{8}$')

def decode(data):
    runs = {}
    for line in data.splitlines():
        if not PAT.fullmatch(line):
            continue
        if zlib.crc32(line[:78]) != int(line[79:87], 16):
            continue
        run, seq, status, error, size, mismatch, source, dest = (int(x, 16) for x in line.split()[1:9])
        record = (status, error, size, mismatch, source, dest)
        runs.setdefault(run, {}).setdefault(seq, set()).add(record)
    output = []
    for run, records in runs.items():
        entry = {'run': f'{run:08X}', 'result': 'incomplete', 'records': {str(k): [list(x) for x in sorted(v)] for k, v in sorted(records.items())}}
        if any(len(v) != 1 for v in records.values()):
            entry['result'] = 'conflict'
        else:
            for seq, value in sorted(records.items()):
                status, count, size, mismatch, source, dest = next(iter(value))
                if count != seq or any((size, mismatch, source, dest)) or seq not in (1, 2, 3):
                    continue
                if set(records) != set(range(seq + 1)):
                    break
                rows = [next(iter(records[i])) for i in range(seq)]
                if status == 0 and seq == 3 and all(s == 0 and e == 0 and b == 4096 and m == 0 and a == d for s,e,b,m,a,d in rows):
                    entry['result'] = 'pass'
                elif status != 0 and rows[-1][0] == status and all(r[0] == 0 for r in rows[:-1]):
                    entry['result'] = 'failure'
                break
        output.append(entry)
    return output

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('log', type=Path)
    a = p.parse_args()
    result = decode(a.log.read_bytes())
    print(json.dumps(result, ensure_ascii=False, indent=2))
    raise SystemExit(0 if result and result[-1]['result'] == 'pass' else 1)

if __name__ == '__main__':
    main()
