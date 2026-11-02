#!/usr/bin/env python3
"""Build a local FAT32 recovery ESP from separately supplied recovery files."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from pyfatfs.PyFatFS import PyFatFS

parser = argparse.ArgumentParser()
parser.add_argument('--files', type=Path, required=True)
parser.add_argument('--boot-wim', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--expected-wim-size', type=int, default=213536655)
parser.add_argument('--expected-files-bytes', type=int, default=22748650,
                    help='Expected total size of the 87 input files; override for an explicitly replaced BCD')
parser.add_argument('--description', default='Original Microsoft recovery environment')
args = parser.parse_args()
required = [
    'EFI/Boot/bootarm.efi', 'EFI/Boot/en-us/bootarm.efi.mui',
    'EFI/Microsoft/Boot/BCD', 'EFI/Microsoft/Boot/Resources/bootres.dll',
    'EFI/Microsoft/Boot/Fonts/segmono_boot.ttf',
    'EFI/Microsoft/Boot/Fonts/segoe_slboot.ttf',
    'EFI/Microsoft/Boot/Fonts/wgl4_boot.ttf', 'Boot/boot.sdi',
]
for name in required:
    if not (args.files / name).is_file():
        raise SystemExit(f'Missing input: {name}')
inputs = sorted(p for p in args.files.rglob('*') if p.is_file())
if len(inputs) != 87 or sum(p.stat().st_size for p in inputs) != args.expected_files_bytes:
    raise SystemExit('Recovery file set is incomplete')
sources = [(p, p.relative_to(args.files).as_posix()) for p in inputs]
sources.append((args.boot_wim, 'sources/boot.wim'))
if args.boot_wim.stat().st_size != args.expected_wim_size:
    raise SystemExit('boot.wim is not the expected complete archive member')
with args.boot_wim.open('rb') as stream:
    if stream.read(8) != b'MSWIM\x00\x00\x00':
        raise SystemExit('boot.wim has an unexpected header')
if args.output.exists():
    raise SystemExit('Output already exists')
args.output.parent.mkdir(parents=True, exist_ok=True)
with args.output.open('xb') as image:
    image.truncate(256 * 1024 * 1024)
subprocess.run(['mkfs.vfat', '-F', '32', '-n', 'RTRECOVERY', str(args.output)], check=True)
records = []
with PyFatFS(str(args.output), preserve_case=True) as fat:
    for source, name in sources:
        target = '/' + name
        directory = target.rsplit('/', 1)[0]
        fat.makedirs(directory, recreate=True)
        digest = hashlib.sha256()
        with source.open('rb') as src, fat.openbin(target, 'w') as dst:
            while chunk := src.read(1024 * 1024):
                dst.write(chunk)
                digest.update(chunk)
        check = hashlib.sha256()
        with fat.openbin(target, 'r') as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                check.update(chunk)
        if digest.digest() != check.digest():
            raise RuntimeError(f'FAT readback mismatch: {name}')
        records.append({'path': name, 'size': source.stat().st_size, 'sha256': digest.hexdigest()})
        print(name, digest.hexdigest(), flush=True)
repair = subprocess.run(['fsck.vfat', '-a', str(args.output)])
if repair.returncode not in (0, 1):
    raise SystemExit('FAT metadata check failed')
subprocess.run(['fsck.vfat', '-n', str(args.output)], check=True)
with args.output.open('rb') as image:
    image_sha256 = hashlib.file_digest(image, 'sha256').hexdigest()
manifest = args.output.with_suffix(args.output.suffix + '.json')
with manifest.open('x') as stream:
    json.dump({'size': args.output.stat().st_size, 'image_sha256': image_sha256, 'files': records,
               'note': args.description}, stream, indent=2)
    stream.write('\n')
print('Created', args.output, 'and', manifest)
