#!/usr/bin/env python3
"""Copy a known WinPE FAT ESP and replace only its local boot.wim."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import struct
from pyfatfs.PyFatFS import PyFatFS

p = argparse.ArgumentParser()
p.add_argument('--source-esp', type=Path, required=True)
p.add_argument('--source-sha256', required=True)
p.add_argument('--verified-winpe-bcd-sha256', required=True,
               help='BCD hash from a template already proven to boot WinPE on this device')
p.add_argument('--wim', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--stage-manifest', type=Path, required=True)
a = p.parse_args()
def digest(stream):
    h = hashlib.sha256()
    for chunk in iter(lambda: stream.read(1024 * 1024), b''):
        h.update(chunk)
    return h.hexdigest()
def filehash(path):
    with path.open('rb') as f:
        return digest(f)
def validate_boot_wim(stream):
    header=stream.read(128)
    if len(header)<128 or header[:8]!=b'MSWIM\0\0\0':
        raise SystemExit('Invalid WIM header')
    header_size,=struct.unpack_from('<I',header,8)
    image_count,=struct.unpack_from('<I',header,44)
    boot_index,=struct.unpack_from('<I',header,120)
    if header_size<208 or image_count != 1 or boot_index != 1:
        raise SystemExit(f'Expected single-image WinPE WIM with boot index 1; count={image_count}, boot={boot_index}')
    return boot_index
with a.wim.open('rb') as stream:
    validate_boot_wim(stream)
if a.output.exists() or a.output.with_suffix(a.output.suffix+'.json').exists():
    raise SystemExit('Refusing to overwrite output or manifest')
if filehash(a.source_esp) != a.source_sha256.lower():
    raise SystemExit('Source ESP hash mismatch')
stage = json.loads(a.stage_manifest.read_text())
import re
if stage.get('version') != 1 or not re.fullmatch(r'[0-9a-f]{32}', stage.get('token', '')) or stage.get('wim_sha256') != filehash(a.wim):
    raise SystemExit('Invalid stage manifest or WIM mismatch')
key_paths = ('/EFI/Boot/bootarm.efi', '/EFI/Microsoft/Boot/BCD', '/Boot/boot.sdi')
with PyFatFS(str(a.source_esp), preserve_case=True, read_only=True) as fat:
    before = {}
    for name in key_paths:
        with fat.openbin(name, 'r') as f:
            before[name] = digest(f)
    if before['/EFI/Microsoft/Boot/BCD'] != a.verified_winpe_bcd_sha256.lower():
        raise SystemExit('BCD does not match the verified WinPE template')
    matches = [name for name in fat.listdir('/sources') if name.lower() == 'boot.wim']
    if len(matches) != 1:
        raise SystemExit('Expected exactly one boot.wim in /sources')
    wim_path = '/sources/' + matches[0]
a.output.parent.mkdir(parents=True, exist_ok=True)
# Refuse an inconsistent source; fsck is strictly read-only throughout.
subprocess.run(['fsck.vfat', '-n', str(a.source_esp)], check=True)
shutil.copyfile(a.source_esp, a.output)
# pyfatfs 1.1.0 does not update FAT32 FSInfo after allocating/freeing
# clusters. Mark the advisory free count unknown before changing files, so
# the filesystem consumer will calculate it rather than trust stale data.
# This is image construction metadata, not fsck repair of an existing volume.
with a.output.open('r+b') as image:
    boot = image.read(512)
    bps = struct.unpack_from('<H', boot, 11)[0]
    reserved = struct.unpack_from('<H', boot, 14)[0]
    fsinfo = struct.unpack_from('<H', boot, 48)[0]
    backup = struct.unpack_from('<H', boot, 50)[0]
    if bps != 512 or not 0 < fsinfo < reserved or boot[510:512] != b'\x55\xaa':
        raise SystemExit('Unsupported FAT32 FSInfo geometry')
    for sector in {fsinfo, backup + fsinfo}:
        if sector >= reserved:
            raise SystemExit('Invalid backup FSInfo location')
        image.seek(sector * bps)
        info = image.read(bps)
        if len(info) != bps or info[:4] != b'RRaA' or info[484:488] != b'rrAa' or info[508:512] != b'\x00\x00\x55\xaa':
            raise SystemExit('Invalid FAT32 FSInfo signature')
        image.seek(sector * bps + 488)
        image.write(struct.pack('<II', 0xffffffff, 0xffffffff))
with PyFatFS(str(a.output), preserve_case=True) as fat:
    fat.remove(wim_path)
    with a.wim.open('rb') as src, fat.openbin(wim_path, 'w') as dst:
        shutil.copyfileobj(src, dst, 1024 * 1024)
with PyFatFS(str(a.output), preserve_case=True, read_only=True) as fat:
    for name, expected in before.items():
        with fat.openbin(name, 'r') as f:
            if digest(f) != expected:
                raise SystemExit(f'Preserved file mismatch: {name}')
    with fat.openbin(wim_path, 'r') as f:
        validate_boot_wim(f)
        f.seek(0)
        if digest(f) != filehash(a.wim):
            raise SystemExit('WIM mismatch')
subprocess.run(['fsck.vfat', '-n', str(a.output)], check=True)
result = {'version': 1, 'token': stage['token'], 'source_esp_sha256': a.source_sha256.lower(), 'wim_sha256': filehash(a.wim),
          'image_sha256': filehash(a.output), 'wim_boot_index': 1, 'preserved_file_sha256': before,
          'note': 'Local derivative only; target boot, PnP, signatures and sensor data unverified.'}
a.output.with_suffix(a.output.suffix+'.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
