#!/usr/bin/env python3
"""Write a verified 256 MiB ESP to the designated partition in recovery."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--image', type=Path, required=True)
p.add_argument('--manifest', type=Path, required=True)
p.add_argument('--flash-log', type=Path, required=True)
p.add_argument('--reboot', action='store_true')
a = p.parse_args()
size = 268435456
m = json.loads(a.manifest.read_text())
if (a.image.stat().st_size != size or m.get('size') != size or
        not re.fullmatch(r'[0-9a-f]{64}', m.get('image_sha256', ''))):
    raise SystemExit('ESPのサイズまたはmanifestが一致しません')
h = hashlib.sha256()
with a.image.open('rb') as f:
    for b in iter(lambda: f.read(4 * 1024 * 1024), b''):
        h.update(b)
if h.hexdigest() != m['image_sha256']:
    raise SystemExit('ESPのSHA-256が一致しません')

def adb(*args):
    return subprocess.check_output(['adb', *args], text=True).strip()

if adb('get-state') != 'recovery' or adb('shell', 'id', '-u') != '0':
    raise SystemExit('rootのAndroid recoveryが必要です')
if (adb('shell', 'readlink', '/dev/block/by-name/EFI') != '/dev/block/mmcblk0p18' or
        adb('shell', 'cat', '/sys/class/block/mmcblk0p18/start') != '360448' or
        adb('shell', 'cat', '/sys/class/block/mmcblk0p18/size') != '524288'):
    raise SystemExit('EFI領域の名前または範囲が一致しません')
device_id = adb('shell', 'cat', '/sys/class/block/mmcblk0p18/dev')
if not re.fullmatch(r'\d+:\d+', device_id):
    raise SystemExit('EFI領域のデバイス番号を取得できません')
if any(len(fields) > 2 and fields[2] == device_id
       for fields in (line.split() for line in adb('shell', 'cat', '/proc/self/mountinfo').splitlines())):
    raise SystemExit('EFI領域がマウントされています')

def require_no_journal():
    guard = Path(__file__).with_name('check-esp-journal.sh')
    with guard.open('rb') as source:
        result = subprocess.run(['adb', 'shell', 'sh', '-s'], stdin=source,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    lines = result.stdout.decode('utf-8', errors='replace').splitlines()
    if result.returncode or lines != ['A04_ESP_JOURNAL_ABSENT']:
        raise SystemExit('ESP復旧ジャーナルが存在するか、状態を確認できません')

require_no_journal()
remote = '/tmp/a04br3-verified-esp.img'
a.flash_log.parent.mkdir(parents=True, exist_ok=True)
with a.flash_log.open('a+b') as log:
    log.seek(0, 2)
    log.write(f'\nESP image SHA256 {h.hexdigest()} bytes {size}\n'.encode())
    log.flush()
    subprocess.run(['adb', 'push', str(a.image), remote], stdout=log, stderr=log, check=True)
    remote_hash = adb('shell', 'sha256sum', remote).split()[0]
    if remote_hash != h.hexdigest():
        raise SystemExit('recoveryに転送したESPのSHA-256が一致しません')
    require_no_journal()
    log.seek(0, 2)
    offset = log.tell()
    result = subprocess.run(['adb', 'shell', 'dd', f'if={remote}',
        'of=/dev/block/by-name/EFI', 'bs=1048576', 'conv=fsync'], stdout=log, stderr=log)
    log.flush()
    log.seek(offset)
    output = log.read().decode('utf-8', errors='replace')
if result.returncode or f'{size} bytes' not in output:
    raise SystemExit('ESP書込みが未完了です。ログを確認してください')
if a.reboot:
    subprocess.run(['adb', 'reboot'], check=True)
print('ESP書込み完了。通常書込み後の生デバイス読戻しは行っていません。')
