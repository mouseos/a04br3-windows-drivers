#!/usr/bin/env python3
"""Describe historical files without replaying hives or changing their contents."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct

WORKSPACE = Path(__file__).resolve().parents[4]


def snapshot(name):
    path = Path(name).resolve(strict=True)
    if not path.is_relative_to(WORKSPACE) or not path.is_file():
        raise ValueError('input must be a file inside the workspace')
    digest = hashlib.sha256()
    with path.open('rb') as f:
        before = os.fstat(f.fileno())
        header = f.read(4096)
        digest.update(header)
        for chunk in iter(lambda: f.read(1024 * 1024), b''):
            digest.update(chunk)
        after = os.fstat(f.fileno())
    if (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns):
        raise ValueError('input changed during reading')
    result = {'path': str(path.relative_to(WORKSPACE)), 'bytes': before.st_size, 'sha256': digest.hexdigest()}
    if len(header) >= 4096 and header[:4] == b'regf':
        primary, secondary = struct.unpack_from('<II', header, 4)
        major, minor = struct.unpack_from('<II', header, 0x14)
        result['regf'] = {'primary_sequence': primary, 'secondary_sequence': secondary,
                          'version': f'{major}.{minor}', 'sequences_equal': primary == secondary}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('files', nargs='+')
    args = parser.parse_args()
    try:
        result = {'scope': 'historical snapshots only; no current device state inferred',
                  'effective_signature_policy': 'unknown', 'recovery_logs_replayed': False,
                  'files': [snapshot(p) for p in args.files]}
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(json.dumps(result, indent=2, sort_keys=True))

if __name__ == '__main__':
    main()
