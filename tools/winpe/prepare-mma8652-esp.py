#!/usr/bin/env python3
"""Copy a known WinPE FAT ESP and replace only its local boot.wim."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
from pyfatfs.PyFatFS import PyFatFS

p = argparse.ArgumentParser()
p.add_argument('--source-esp', type=Path, required=True)
p.add_argument('--source-sha256', required=True)
p.add_argument('--wim', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
def digest(stream):
    h = hashlib.sha256()
    for chunk in iter(lambda: stream.read(1024 * 1024), b''):
        h.update(chunk)
    return h.hexdigest()
def filehash(path):
    with path.open('rb') as f:
        return digest(f)
if a.output.exists() or a.output.with_suffix(a.output.suffix+'.json').exists():
    raise SystemExit('Refusing to overwrite output or manifest')
if filehash(a.source_esp) != a.source_sha256.lower():
    raise SystemExit('Source ESP hash mismatch')
key_paths = ('/EFI/Boot/bootarm.efi', '/EFI/Microsoft/Boot/BCD', '/Boot/boot.sdi')
with PyFatFS(str(a.source_esp), preserve_case=True, read_only=True) as fat:
    before = {}
    for name in key_paths:
        with fat.openbin(name, 'r') as f:
            before[name] = digest(f)
a.output.parent.mkdir(parents=True, exist_ok=True)
shutil.copyfile(a.source_esp, a.output)
with PyFatFS(str(a.output), preserve_case=True) as fat:
    fat.remove('/sources/boot.wim')
    with a.wim.open('rb') as src, fat.openbin('/sources/boot.wim', 'w') as dst:
        shutil.copyfileobj(src, dst, 1024 * 1024)
with PyFatFS(str(a.output), preserve_case=True, read_only=True) as fat:
    for name, expected in before.items():
        with fat.openbin(name, 'r') as f:
            if digest(f) != expected:
                raise SystemExit(f'Preserved file mismatch: {name}')
    with fat.openbin('/sources/boot.wim', 'r') as f:
        if digest(f) != filehash(a.wim):
            raise SystemExit('WIM mismatch')
repair = subprocess.run(['fsck.vfat', '-a', str(a.output)])
if repair.returncode not in (0, 1):
    raise SystemExit('FAT repair failed')
subprocess.run(['fsck.vfat', '-n', str(a.output)], check=True)
result = {'source_esp_sha256': a.source_sha256.lower(), 'wim_sha256': filehash(a.wim),
          'image_sha256': filehash(a.output), 'preserved_file_sha256': before,
          'note': 'Local derivative only; target boot, PnP, signatures and sensor data unverified.'}
a.output.with_suffix(a.output.suffix+'.json').write_text(json.dumps(result, indent=2)+'\n')
print(json.dumps(result, indent=2))
