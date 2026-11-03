#!/usr/bin/env python3
"""Apply a separately supplied serviced Windows WIM to a new local NTFS image."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from ntfs_root_attributes import normalize_root

p = argparse.ArgumentParser()
p.add_argument('--wim', type=Path, required=True)
p.add_argument('--expected-wim-sha256', required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--wimlib', default='wimlib-imagex')
p.add_argument('--driver-source-commit', required=True)
a = p.parse_args()
sectors, start, sector_bytes = 29584384, 917504, 512
partial = a.output.with_name(a.output.name + '.partial')
manifest = a.output.with_name(a.output.name + '.json')
if a.output.exists() or partial.exists() or manifest.exists():
    raise SystemExit('Output, partial output, or manifest already exists')
if not a.wim.is_file():
    raise SystemExit('WIM input is missing')

def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()

source_hash = sha(a.wim)
if source_hash.lower() != a.expected_wim_sha256.lower():
    raise SystemExit('WIM hash mismatch')
subprocess.run([a.wimlib, 'verify', str(a.wim)], check=True)
a.output.parent.mkdir(parents=True, exist_ok=True)
with partial.open('xb') as stream:
    stream.truncate(sectors * sector_bytes)
subprocess.run(['mkntfs', '-F', '-Q', '-L', 'Windows', '-s', '512', '-p', str(start),
                '-H', '255', '-S', '63', str(partial), str(sectors)], check=True)
subprocess.run([a.wimlib, 'apply', str(a.wim), '1', str(partial), '--strict-acls'], check=True)
root_before, root_after = normalize_root(partial, sectors * sector_bytes, start)
subprocess.run(['ntfsfix', '-n', str(partial)], check=True)
with partial.open('rb') as stream:
    boot = stream.read(512)
if (boot[3:11] != b'NTFS    ' or struct.unpack_from('<H', boot, 11)[0] != sector_bytes or
        struct.unpack_from('<I', boot, 28)[0] != start or boot[510:512] != b'\x55\xaa'):
    raise SystemExit('NTFS boot sector geometry mismatch')
image_hash = sha(partial)
partial.rename(a.output)
manifest.write_text(json.dumps({'image': str(a.output), 'size_bytes': sectors * sector_bytes,
    'sha256': image_hash, 'partition_start_lba': start, 'sector_bytes': sector_bytes,
    'source_wim_sha256': source_hash, 'image_index': 1,
    'ntfs_root_attributes_before': root_before, 'ntfs_root_attributes_after': root_after,
    'driver_source_commit': a.driver_source_commit,
    'validation': 'wimlib verify; strict ACL apply; normalized root attributes; ntfsfix -n; NTFS boot geometry',
    'limitations': 'experimental; OS boot and production driver signing are not established'}, indent=2) + '\n')
print('NTFS image built and verified:', a.output)
