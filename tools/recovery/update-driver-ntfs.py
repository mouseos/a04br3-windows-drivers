#!/usr/bin/env python3
"""実験用の同一INFドライバをrecoveryからファイル単位で更新する。"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

p = argparse.ArgumentParser()
p.add_argument('--package', type=Path, required=True)
p.add_argument('--ntfs3g', type=Path, required=True, help='端末で実行できる静的ntfs-3g')
p.add_argument('--backup-dir', type=Path, required=True, help='新規作成するバックアップ先')
a = p.parse_args()
files = ('a04msdcprobe.sys', 'a04msdcprobe.cat', 'a04-msdc-probe.inf')
if a.backup_dir.exists() or not a.ntfs3g.is_file() or any(not (a.package / n).is_file() for n in files):
    raise SystemExit('入力不足またはバックアップ先が既にあります')

def run(*args, capture=False):
    return subprocess.run(args, check=True, text=True, stdout=subprocess.PIPE if capture else None).stdout

def adb(*args):
    return run('adb', *args, capture=True).strip()

def shell(command):
    return adb('shell', '-T', command)

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

if adb('get-state') != 'recovery' or adb('shell', 'id', '-u') != '0':
    raise SystemExit('rootのAndroid recoveryが必要です')
if (adb('shell', 'readlink', '/dev/block/by-name/Windows') != '/dev/block/mmcblk0p20' or
    adb('shell', 'cat', '/sys/class/block/mmcblk0p20/start') != '917504' or
    adb('shell', 'cat', '/sys/class/block/mmcblk0p20/size') != '29584384'):
    raise SystemExit('Windows領域の位置が一致しません')
device_id = adb('shell', 'cat', '/sys/class/block/mmcblk0p20/dev')
if not re.fullmatch(r'\d+:\d+', device_id) or any(
    len(x) > 2 and x[2] == device_id for x in
    (line.split() for line in adb('shell', 'cat', '/proc/self/mountinfo').splitlines())):
    raise SystemExit('Windows領域がマウントされているかデバイス番号が不正です')

remote = '/tmp/a04-driver-update'
mount = '/tmp/a04-windows-update'
repo = 'Windows/System32/DriverStore/FileRepository/a04-msdc-probe.inf_arm_6c359506e5826fef'
catroot = 'Windows/System32/catroot/{F750E6C3-38EE-11D1-85E5-00C04FC295EE}'
targets = [('active.sys', 'Windows/System32/Drivers/a04msdcprobe.sys', 'a04msdcprobe.sys'),
           ('store.sys', f'{repo}/a04msdcprobe.sys', 'a04msdcprobe.sys'),
           ('store.cat', f'{repo}/a04msdcprobe.cat', 'a04msdcprobe.cat'),
           ('catroot.cat', f'{catroot}/oem32.cat', 'a04msdcprobe.cat')]
shell(f'mkdir -p {remote} {mount}')
run('adb', 'push', str(a.ntfs3g), f'{remote}/ntfs-3g')
if shell(f'sha256sum {remote}/ntfs-3g').split()[0] != digest(a.ntfs3g):
    raise SystemExit('転送されたntfs-3gのハッシュが不一致です')
for n in files:
    run('adb', 'push', str(a.package / n), f'{remote}/{n}')
for n in files:
    if shell(f'sha256sum {remote}/{n}').split()[0] != digest(a.package / n):
        raise SystemExit(f'転送された {n} のハッシュが不一致です')
shell(f'chmod 755 {remote}/ntfs-3g')
mounted = False
try:
    shell(f'{remote}/ntfs-3g -o ro /dev/block/by-name/Windows {mount}')
    mounted = True
    inf_hash = digest(a.package / 'a04-msdc-probe.inf')
    for path in (f'{repo}/a04-msdc-probe.inf', 'Windows/inf/oem32.inf'):
        if shell(f'sha256sum "{mount}/{path}"').split()[0] != inf_hash:
            raise RuntimeError('インストール済みINFがパッケージと異なります')
    a.backup_dir.mkdir(parents=True)
    for name, target, _ in targets:
        run('adb', 'pull', f'{mount}/{target}', str(a.backup_dir / name))
    (a.backup_dir / 'manifest.json').write_text(json.dumps({
        'package_sha256': {n: digest(a.package / n) for n in files},
        'backup_sha256': {n: digest(a.backup_dir / n) for n, _, _ in targets},
        'limitations': 'Experimental test-signed package; catalog database and Windows boot must be verified.'
    }, indent=2) + '\n')
    shell(f'umount {mount}')
    mounted = False
    shell(f'{remote}/ntfs-3g /dev/block/by-name/Windows {mount}')
    mounted = True
    # Preserve existing file security metadata; the host backup permits recovery.
    commands = [f'cp "{remote}/{source}" "{mount}/{target}"' for _, target, source in targets]
    shell('set -e; ' + '; '.join(commands) + '; sync')
    shell(f'umount {mount}')
    mounted = False
    print(f'更新完了。バックアップ: {a.backup_dir}。Windowsの署名受理と起動は未検証です。')
except (subprocess.CalledProcessError, OSError, RuntimeError) as exc:
    print(f'更新に失敗しました: {exc}。バックアップを保持し、Windowsを起動しないでください。', file=sys.stderr)
    raise
finally:
    if mounted:
        subprocess.run(['adb', 'shell', '-T', f'umount {mount}'], check=False)
