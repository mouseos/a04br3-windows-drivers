#!/usr/bin/env python3
"""保存済み実験NTFSの初期レジストリ群をrecoveryから復元する。"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tarfile
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--baseline-dir', type=Path, required=True)
p.add_argument('--ntfs3g', type=Path, required=True)
p.add_argument('--backup-dir', type=Path, required=True)
a = p.parse_args()
manifest = json.loads((a.baseline_dir / 'manifest.json').read_text())
files = manifest['files']
pattern = re.compile(r'^(?:SYSTEM|SOFTWARE|SAM|SECURITY|DEFAULT|COMPONENTS|DRIVERS|BBI|ELAM)(?:$|\.[A-Za-z0-9]+$|\{[0-9A-Fa-f-]+\}\.TM(?:\.blf|Container[0-9]+\.regtrans-ms)$)')
if a.backup_dir.exists() or not a.ntfs3g.is_file() or 'State.ini' not in files or not files:
    raise SystemExit('入力または新規バックアップ先が不正です')
for name, sha in files.items():
    if (name != 'State.ini' and not pattern.fullmatch(name)) or not re.fullmatch(r'[0-9a-f]{64}', sha):
        raise SystemExit('manifestに不正なファイル名またはハッシュがあります')
    if hashlib.sha256((a.baseline_dir / name).read_bytes()).hexdigest() != sha:
        raise SystemExit(f'候補ファイルが不一致です: {name}')

def adb(*args, capture=False):
    return subprocess.run(['adb', *args], check=True, text=True,
                          stdout=subprocess.PIPE if capture else None).stdout

def shell(cmd):
    return adb('shell', '-T', cmd, capture=True).strip()

if adb('get-state', capture=True).strip() != 'recovery' or shell('id -u') != '0':
    raise SystemExit('root recoveryが必要です')
if (shell('readlink /dev/block/by-name/Windows') != '/dev/block/mmcblk0p20' or
    shell('cat /sys/class/block/mmcblk0p20/start') != '917504' or
    shell('cat /sys/class/block/mmcblk0p20/size') != '29584384'):
    raise SystemExit('Windows領域の位置が一致しません')
device_id = shell('cat /sys/class/block/mmcblk0p20/dev')
mountinfo = shell('cat /proc/self/mountinfo')
if not re.fullmatch(r'\d+:\d+', device_id) or any(
    len(x) > 2 and x[2] == device_id for x in (line.split() for line in mountinfo.splitlines())):
    raise SystemExit('Windows領域が既にマウントされているかデバイス番号が不正です')

mount, stage = '/tmp/a04-setup-volume', '/tmp/a04-setup-baseline'
config = 'Windows/System32/config'
names = sorted(n for n in files if n != 'State.ini')
with tempfile.TemporaryDirectory(prefix='a04-setup-tar-') as tmp:
    archive = Path(tmp) / 'baseline.tar.gz'
    with tarfile.open(archive, 'w:gz') as t:
        for n in sorted(files):
            t.add(a.baseline_dir / n, arcname=n, recursive=False)
    shell(f'mkdir -p {mount} {stage}')
    adb('push', str(a.ntfs3g), f'{stage}/ntfs-3g')
    adb('push', str(archive), f'{stage}/baseline.tar.gz')
    for local, remote in ((a.ntfs3g, 'ntfs-3g'), (archive, 'baseline.tar.gz')):
        if shell(f'sha256sum {stage}/{remote}').split()[0] != hashlib.sha256(local.read_bytes()).hexdigest():
            raise SystemExit(f'転送ハッシュ不一致: {remote}')
    shell(f'chmod 755 {stage}/ntfs-3g; tar -xzf {stage}/baseline.tar.gz -C {stage}')
    mounted = False
    try:
        shell(f'{stage}/ntfs-3g -o ro /dev/block/by-name/Windows {mount}')
        mounted = True
        live = shell(f'ls -1 {mount}/{config}').splitlines()
        if set(n for n in live if pattern.fullmatch(n)) != set(names):
            raise RuntimeError('実機の関連ハイブ一覧が元イメージと異なります')
        a.backup_dir.mkdir(parents=True)
        for n in names:
            adb('pull', f'{mount}/{config}/{n}', str(a.backup_dir / n))
        adb('pull', f'{mount}/Windows/Setup/State/State.ini', str(a.backup_dir / 'State.ini'))
        (a.backup_dir / 'manifest.json').write_text(json.dumps({n: hashlib.sha256(
            (a.backup_dir / n).read_bytes()).hexdigest() for n in files}, indent=2) + '\n')
        shell(f'umount {mount}')
        mounted = False
        shell(f'{stage}/ntfs-3g /dev/block/by-name/Windows {mount}')
        mounted = True
        commands = [f'cp "{stage}/{n}" "{mount}/{config}/{n}"' for n in names]
        commands.append(f'cp {stage}/State.ini {mount}/Windows/Setup/State/State.ini')
        shell('set -e; ' + '; '.join(commands) + '; sync')
        shell(f'umount {mount}')
        mounted = False
        print('初期レジストリ候補を書込みました。セットアップ結果は未確認です。')
    finally:
        if mounted:
            subprocess.run(['adb', 'shell', '-T', f'umount {mount}'], check=False)
