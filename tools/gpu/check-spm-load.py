#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate observed fixed-probe SPM allocation extents, without exposing addresses."""
import argparse
import hashlib
import json
import pathlib
import sys


def analyze(events):
    maps = {}
    checked = 0
    for e in events:
        op = e['event']
        if op == 'vm_map':
            key = (e['vm'], e['va'])
            if key in maps or e['size'] <= 0:
                raise ValueError('duplicate or empty mapping')
            maps[key] = e['size']
        elif op == 'vm_unmap':
            key = (e['vm'], e['va'])
            if maps.pop(key) != e['size']:
                raise ValueError('unmap extent mismatch')
        elif op == 'vm_destroy':
            maps = {k: v for k, v in maps.items() if k[0] != e['handle']}
        elif op == 'spm_load':
            if ((e['width'], e['height'], e['samples'], e['outputs'], e['tiles']) != (64, 64, 1, 1, 0)
                    or (e['scratch_size'], e['used'], e['constants_size'], e['pds_size']) != (16384, 16384, 32, 16)
                    or e['pds_gpu'] >> 32 != 0xda or e['bg1'] != (e['pds_gpu'] & 0xffffffff) << 32):
                raise ValueError('unexpected SPM layout')
            owners = []
            for addr, size in ((e['scratch_gpu'], e['scratch_size']),
                               (e['constants_gpu'], e['constants_size']),
                               (e['pds_gpu'], e['pds_size'])):
                found = [k for k, n in maps.items() if addr >= k[1] and size <= n and addr - k[1] <= n - size]
                if len(found) != 1:
                    raise ValueError('SPM range is not uniquely mapped')
                owners.append(found[0])
            if len({k[0] for k in owners}) != 1:
                raise ValueError('SPM ranges belong to different VMs')
            checked += 1
    if checked != 3:
        raise ValueError('incomplete fixed probe')
    return {'spm_loads': checked, 'scratch_bytes': 16384, 'constants_bytes': 32,
            'pds_bytes': 16, 'gpu_execution_allowed': False,
            'limitations': ['mapping checked at construction only; job association and completion lifetime are unproven',
                            'texture descriptors, USC, hardware accesses and cache synchronization are unproven']}


def check(root):
    root = pathlib.Path(root)
    m = json.loads((root / 'run-manifest.json').read_text())
    raw = (root / 'events.jsonl').read_bytes()
    if (not m.get('scratchless_vertex') or not (m.get('diagnostic_manifest') or {}).get('spm_load_metadata_only')
            or hashlib.sha256(raw).hexdigest() != m.get('events_sha256')):
        raise ValueError('capture provenance')
    return analyze([json.loads(line) for line in raw.splitlines()])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('capture')
    try:
        print(json.dumps(check(p.parse_args().capture), indent=2))
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError):
        print('invalid SPM capture', file=sys.stderr)
        return 2
    return 1  # Observation is not GPU execution approval.


if __name__ == '__main__':
    sys.exit(main())
