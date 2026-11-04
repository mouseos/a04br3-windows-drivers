#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Inspect legacy shim metadata without publishing BO bytes or pointers.

This format does not record enough to produce an executable fixture. The
command deliberately has no export option; see README-render-capture.md.
"""
import argparse
import hashlib
import json
import pathlib
import re
import struct
import sys

MAP = re.compile(r'map=(\d+) handle=(\d+) va=([0-9a-f]+) offset=([0-9a-f]+) size=([0-9a-f]+)\Z')
JOB = re.compile(r'job=(\d+) type=(\d+) flags=(\d+) size=(\d+) stream=([0-9a-f]*)\Z')
HWRT = re.compile(r'size=(\d+)x(\d+) samples=(\d+) layers=(\d+) region=(\d+) merge=([0-9a-f,]+)\Z')
LIMIT = 1 << 40


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for data in iter(lambda: f.read(65536), b''):
            h.update(data)
    return h.digest()


def inspect(root, submit):
    root = pathlib.Path(root)
    if submit < 0:
        raise ValueError('negative submit')
    maps, jobs, indices = [], [], set()
    for line in (root / f'submit-{submit}.txt').read_text(encoding='ascii').splitlines():
        m = MAP.fullmatch(line)
        j = JOB.fullmatch(line)
        if m:
            index, handle = map(int, m.group(1, 2))
            va, offset, size = (int(x, 16) for x in m.group(3, 4, 5))
            if index in indices or not size or va >= LIMIT or size > LIMIT - va or offset + size > (1 << 64):
                raise ValueError('invalid mapping')
            indices.add(index)
            path = root / f'submit-{submit}-map-{index}.bin'
            if not path.is_file() or path.is_symlink() or path.stat().st_size != size:
                raise ValueError('missing or truncated mapping data')
            maps.append((va, size, offset, handle, digest(path)))
        elif j:
            index, kind, flags, size = map(int, j.group(1, 2, 3, 4))
            data = bytes.fromhex(j.group(5))
            if index != len(jobs) or size != len(data):
                raise ValueError('invalid job stream')
            jobs.append((kind, flags, data))
        else:
            raise ValueError('unrecognized capture record')
    if not maps or not jobs:
        raise ValueError('empty mappings or jobs')
    exact_aliases = 0
    unique = []
    for mapping in sorted(maps):
        va, size, offset, handle, checksum = mapping
        if unique and va < unique[-1][0] + unique[-1][1]:
            if mapping != unique[-1]:
                raise ValueError('overlapping non-identical mappings')
            exact_aliases += 1
        else:
            unique.append(mapping)
    hwrt = (root / 'hwrt.txt').read_text(encoding='ascii').splitlines()
    if not hwrt or any(not HWRT.fullmatch(line) for line in hwrt):
        raise ValueError('unrecognized HWRT metadata')
    for line in hwrt:
        if len(HWRT.fullmatch(line).group(6).split(',')) != 6:
            raise ValueError('invalid HWRT merge coefficients')
    # Geometry stream field: VDM control stream base (the TPU field is a
    # register encoding, not necessarily an absolute GPU virtual address).
    # Checking that first byte lies in a map is NOT a range or dependency proof.
    checked = missing = 0
    for kind, _, data in jobs:
        if kind == 0:
            if len(data) < 24 or not 24 <= struct.unpack_from('<I', data)[0] <= len(data):
                raise ValueError('invalid geometry stream')
            for addr in (struct.unpack_from('<Q', data, 8)[0],):
                if addr:
                    checked += 1
                    if not any(va <= addr < va + size for va, size, *_ in unique):
                        missing += 1
    return {'submit': submit, 'jobs': len(jobs), 'maps': len(maps),
            'distinct_maps': len(unique), 'duplicate_maps': exact_aliases,
            'geometry_address_starts_checked': checked,
            'geometry_address_starts_unmapped': missing,
            'hwrt_records': len(hwrt),
            'fixture_exportable': False,
            'blocking_reasons': [
                'legacy capture omits HWRT geometry, target addresses and freelists',
                'job/context/HWRT associations and complete memory dependencies are absent',
                'raw mapped BO contents have no verified safe-byte provenance',
                'the capture does not bind source, build configuration and generated binaries']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=pathlib.Path)
    parser.add_argument('--submit', type=int, default=0)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.capture, args.submit), indent=2))
    except (OSError, ValueError, UnicodeError) as e:
        # Do not print filenames or raw records, which may disclose private data.
        print('capture rejected: ' + type(e).__name__, file=sys.stderr)
        return 2
    return 1  # never mistake a metadata audit for a usable fixture


if __name__ == '__main__':
    sys.exit(main())
