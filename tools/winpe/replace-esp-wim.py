#!/usr/bin/env python3
"""Create a local test ESP from a verified template and an updated WinPE WIM."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from pyfatfs.PyFatFS import PyFatFS

def digest_stream(stream):
    digest = hashlib.sha256()
    for chunk in iter(lambda: stream.read(1024 * 1024), b''):
        digest.update(chunk)
    return digest.hexdigest()

def fat_digest(fat, name):
    with fat.openbin(name, 'r') as stream:
        return digest_stream(stream)

parser = argparse.ArgumentParser()
parser.add_argument('--template', required=True, type=Path)
parser.add_argument('--wim', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
parser.add_argument('--template-bcd-sha256', required=True)
parser.add_argument('--template-wim-sha256', required=True)
args = parser.parse_args()
if args.output.exists() or args.output.resolve() in (args.template.resolve(), args.wim.resolve()):
    raise SystemExit('Output must be a new local image')
if args.template.stat().st_size != 256 * 1024 * 1024:
    raise SystemExit('Unexpected template size')
with args.wim.open('rb') as stream:
    if stream.read(8) != b'MSWIM\x00\x00\x00':
        raise SystemExit('Invalid WIM header')
    stream.seek(0)
    new_wim = digest_stream(stream)
with PyFatFS(str(args.template), read_only=True) as fat:
    if fat_digest(fat, '/EFI/Microsoft/Boot/BCD') != args.template_bcd_sha256 or \
       fat_digest(fat, '/sources/boot.wim') != args.template_wim_sha256:
        raise SystemExit('Template BCD or WIM does not match the expected known image')
args.output.parent.mkdir(parents=True, exist_ok=True)
with args.template.open('rb') as source, args.output.open('xb') as target:
    shutil.copyfileobj(source, target, 1024 * 1024)
with PyFatFS(str(args.output), preserve_case=True) as fat:
    fat.remove('/sources/boot.wim')
    with args.wim.open('rb') as source, fat.openbin('/sources/boot.wim', 'w') as target:
        shutil.copyfileobj(source, target, 1024 * 1024)
    if fat_digest(fat, '/sources/boot.wim') != new_wim or \
       fat_digest(fat, '/EFI/Microsoft/Boot/BCD') != args.template_bcd_sha256:
        raise SystemExit('Updated image verification failed')
repair = subprocess.run(['fsck.vfat', '-a', str(args.output)])
if repair.returncode not in (0, 1):
    raise SystemExit('FAT metadata repair failed')
subprocess.run(['fsck.vfat', '-n', str(args.output)], check=True)
with PyFatFS(str(args.output), read_only=True) as fat:
    if fat_digest(fat, '/sources/boot.wim') != new_wim or \
       fat_digest(fat, '/EFI/Microsoft/Boot/BCD') != args.template_bcd_sha256:
        raise SystemExit('Image verification failed after FAT metadata repair')
with args.output.open('rb') as stream:
    image_hash = digest_stream(stream)
with args.output.with_suffix(args.output.suffix + '.json').open('x') as stream:
    json.dump({'image_sha256': image_hash, 'wim_sha256': new_wim,
               'template_wim_sha256': args.template_wim_sha256,
               'bcd_sha256': args.template_bcd_sha256,
               'note': 'Local combined WinPE test image; no device write implied'}, stream, indent=2)
    stream.write('\n')
print('Created and verified', args.output, image_hash)
