#!/usr/bin/env python3
"""Save two copies of the unused scratch range before BCD export."""
import argparse
import hashlib
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--backup', type=Path, required=True)
p.add_argument('--mirror', type=Path, required=True)
a = p.parse_args()
if a.backup.absolute() == a.mirror.absolute() or a.backup.exists() or a.mirror.exists():
    raise SystemExit('バックアップ先が同一か、既に存在します')

def adb(*args):
    return subprocess.check_output(['adb', *args], text=True).strip()

if (adb('get-state') != 'recovery' or adb('shell', 'id', '-u') != '0' or
        adb('shell', 'readlink', '/dev/block/by-name/Windows') != '/dev/block/mmcblk0p20' or
        adb('shell', 'cat', '/sys/class/block/mmcblk0p20/start') != '917504' or
        adb('shell', 'cat', '/sys/class/block/mmcblk0p20/size') != '29584384'):
    raise SystemExit('root recoveryまたはWindows領域の位置が一致しません')
data = subprocess.check_output(['adb', 'exec-out',
    'dd if=/dev/block/by-name/Windows bs=512 skip=2048 count=128 2>/dev/null'])
if len(data) != 65536 or any(data):
    raise SystemExit('scratch領域が64 KiBのゼロデータではありません')
for path in (a.backup, a.mirror):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as f:
        f.write(data)
    if path.read_bytes() != data:
        raise SystemExit(f'バックアップ照合に失敗しました: {path}')
print('scratchバックアップ SHA-256:', hashlib.sha256(data).hexdigest())
