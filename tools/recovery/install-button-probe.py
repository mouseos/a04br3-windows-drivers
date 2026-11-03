#!/usr/bin/env python3
"""root recoveryから一回限りのWindowsボタン列挙診断を導入する。"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
from Registry import Registry

p = argparse.ArgumentParser()
p.add_argument('--exe', type=Path, required=True)
p.add_argument('--ntfs3g', type=Path, required=True)
p.add_argument('--backup-dir', type=Path, required=True)
a = p.parse_args()
if a.backup_dir.exists() or not a.exe.is_file() or not a.ntfs3g.is_file():
    raise SystemExit('入力不足またはバックアップ先が既にあります')

def run(*args, capture=False, check=True):
    return subprocess.run(args, check=check, text=True,
                          stdout=subprocess.PIPE if capture else None).stdout
def adb(*args):
    return run('adb', *args, capture=True).strip()
def shell(command):
    return adb('shell', '-T', command)
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def registry_values(path):
    values, pending = {}, [Registry.Registry(str(path)).root()]
    while pending:
        node = pending.pop()
        for value in node.values():
            raw = value.raw_data()
            values[(node.path(), value.name())] = (value.value_type(), hashlib.sha256(raw).digest())
        pending.extend(node.subkeys())
    return values

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
    raise SystemExit('Windows領域がマウント中かデバイス番号が不正です')

remote, mount = '/tmp/a04-button-probe', '/tmp/a04-button-win'
shell(f'mkdir -p {remote} {mount}')
run('adb', 'push', str(a.ntfs3g), f'{remote}/ntfs-3g')
run('adb', 'push', str(a.exe), f'{remote}/probe.exe')
for path, name in ((a.ntfs3g, 'ntfs-3g'), (a.exe, 'probe.exe')):
    if shell(f'sha256sum {remote}/{name}').split()[0] != digest(path):
        raise SystemExit('転送ファイルのハッシュが不一致です')
shell(f'chmod 755 {remote}/ntfs-3g')
a.backup_dir.mkdir(parents=True)
mounted = False
try:
    shell(f'{remote}/ntfs-3g -o ro /dev/block/by-name/Windows {mount}')
    mounted = True
    shell(f'test ! -e {mount}/Windows/System32/a04-button-probe.exe')
    run('adb', 'pull', f'{mount}/Windows/System32/config/SYSTEM', str(a.backup_dir / 'SYSTEM.original'))
    shell(f'umount {mount}')
    mounted = False
    hive = a.backup_dir / 'SYSTEM.candidate'
    shutil.copyfile(a.backup_dir / 'SYSTEM.original', hive)
    prefix = r'HKEY_LOCAL_MACHINE\SYSTEM'
    select = a.backup_dir / 'select.reg'
    run('reged', '-x', str(hive), prefix, r'\Select', str(select))
    current = re.search(r'"Current"=dword:([0-9a-fA-F]{8})', select.read_text())
    if not current or not 1 <= int(current.group(1), 16) <= 999:
        raise RuntimeError('現在のControlSetを確定できません')
    key = f'ControlSet{int(current.group(1), 16):03d}\\Services\\A04ButtonProbe'
    existing = a.backup_dir / 'existing.reg'
    run('reged', '-x', str(hive), prefix, '\\' + key, str(existing))
    if f'[{prefix}\\{key}]'.casefold() in existing.read_text().casefold():
        raise RuntimeError('診断サービスが既に存在します')
    patch = a.backup_dir / 'add-service.reg'
    patch.write_text('Windows Registry Editor Version 5.00\n\n'
                     f'[{prefix}\\{key}]\n'
                     '"Type"=dword:00000010\n"Start"=dword:00000002\n'
                     '"ErrorControl"=dword:00000001\n'
                     '"ObjectName"="LocalSystem"\n'
                     '"ImagePath"="C:\\\\Windows\\\\System32\\\\a04-button-probe.exe"\n', encoding='ascii')
    result = subprocess.run(['reged', '-C', '-I', str(hive), prefix, str(patch)])
    if result.returncode not in (0, 2):
        raise RuntimeError('診断サービスのハイブ作成に失敗しました')
    verify = a.backup_dir / 'verify-service.reg'
    run('reged', '-x', str(hive), prefix, '\\' + key, str(verify))
    text = verify.read_text()
    if any(token not in text for token in ('"Start"=dword:00000002',
                                           '"Type"=dword:00000010', 'a04-button-probe.exe')):
        raise RuntimeError('候補ハイブのサービス値が一致しません')
    before, after = registry_values(a.backup_dir / 'SYSTEM.original'), registry_values(hive)
    changes = {k for k in before.keys() | after.keys() if before.get(k) != after.get(k)}
    if (len(changes) != 5 or any(not k[0].endswith('\\' + key) for k in changes) or
            {k[1] for k in changes} != {'Type', 'Start', 'ErrorControl', 'ObjectName', 'ImagePath'}):
        raise RuntimeError('候補ハイブに想定外の値変更があります')
    (a.backup_dir / 'manifest.json').write_text(json.dumps({
        'system_original_sha256': digest(a.backup_dir / 'SYSTEM.original'),
        'system_candidate_sha256': digest(hive), 'exe_sha256': digest(a.exe),
        'note': 'One-shot diagnostic; Windows execution and self-disable are unverified.'}, indent=2) + '\n')
    run('adb', 'push', str(hive), f'{remote}/SYSTEM.candidate')
    if shell(f'sha256sum {remote}/SYSTEM.candidate').split()[0] != digest(hive):
        raise RuntimeError('候補ハイブの転送ハッシュが不一致です')
    shell(f'{remote}/ntfs-3g /dev/block/by-name/Windows {mount}')
    mounted = True
    shell(f'set -e; cp {remote}/probe.exe {mount}/Windows/System32/a04-button-probe.exe; '
          f'cp {remote}/SYSTEM.candidate {mount}/Windows/System32/config/SYSTEM; sync')
    shell(f'umount {mount}')
    mounted = False
    print('診断サービスを導入しました。起動・自己無効化は未検証です。')
finally:
    if mounted:
        subprocess.run(['adb', 'shell', '-T', f'umount {mount}'], check=False)
