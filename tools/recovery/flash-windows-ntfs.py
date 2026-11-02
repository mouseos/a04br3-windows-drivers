#!/usr/bin/env python3
"""Stream a verified image to the designated Windows partition in recovery."""
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
a = p.parse_args()
size, start, sectors = 15147204608, 917504, 29584384
m = json.loads(a.manifest.read_text())
if (m.get('size_bytes') != size or m.get('partition_start_lba') != start or
        m.get('sector_bytes') != 512 or a.image.stat().st_size != size or
        not re.fullmatch(r'[0-9a-f]{64}', m.get('sha256', ''))):
    raise SystemExit('イメージのサイズまたはmanifestが一致しません')
h = hashlib.sha256()
with a.image.open('rb') as f:
    for block in iter(lambda: f.read(4 * 1024 * 1024), b''):
        h.update(block)
if h.hexdigest() != m['sha256']:
    raise SystemExit('イメージのSHA-256が一致しません')

def adb(*args):
    return subprocess.check_output(['adb', *args], text=True).strip()

if adb('get-state') != 'recovery' or adb('shell', 'id', '-u') != '0':
    raise SystemExit('rootのAndroid recoveryが必要です')
if (adb('shell', 'readlink', '/dev/block/by-name/Windows') != '/dev/block/mmcblk0p20' or
        adb('shell', 'cat', '/sys/class/block/mmcblk0p20/start') != str(start) or
        adb('shell', 'cat', '/sys/class/block/mmcblk0p20/size') != str(sectors)):
    raise SystemExit('Windows領域の名前または範囲が一致しません')
device_id = adb('shell', 'cat', '/sys/class/block/mmcblk0p20/dev')
if not re.fullmatch(r'\d+:\d+', device_id):
    raise SystemExit('Windows領域のデバイス番号を取得できません')
if any(len(fields) > 2 and fields[2] == device_id
       for fields in (line.split() for line in adb('shell', 'cat', '/proc/self/mountinfo').splitlines())):
    raise SystemExit('Windows領域がマウントされています')

a.flash_log.parent.mkdir(parents=True, exist_ok=True)
with a.flash_log.open('a+b') as log:
    log.seek(0, 2)
    offset = log.tell()
    log.write(f'\nWindows NTFS image SHA256 {h.hexdigest()} bytes {size}\n'.encode())
    log.flush()
    producer = subprocess.Popen(['pigz', '-1', '-p', '4', '-c', str(a.image)], stdout=subprocess.PIPE, stderr=log)
    assert producer.stdout is not None
    receiver = subprocess.Popen(['adb', 'shell', '-T',
        'sh -c "set -o pipefail; gzip -dc | dd of=/dev/block/by-name/Windows bs=1048576 conv=fsync"'],
        stdin=producer.stdout, stdout=log, stderr=log)
    producer.stdout.close()
    receiver_status = receiver.wait()
    producer_status = producer.wait()
    log.flush()
    log.seek(offset)
    output = log.read().decode('utf-8', errors='replace')
if producer_status or receiver_status or f'{size} bytes' not in output:
    raise SystemExit('転送または書込みが未完了です。ログを確認してください')
print('Windows領域への書込み完了。通常書込み後の生デバイス読戻しは行っていません。')
