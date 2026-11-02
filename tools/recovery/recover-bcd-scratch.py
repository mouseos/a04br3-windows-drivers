#!/usr/bin/env python3
"""Recover a WinPE-generated BCD and restore its backed-up scratch area."""
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import uuid
import zlib

p = argparse.ArgumentParser()
p.add_argument('--backup', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--disk-guid', required=True)
p.add_argument('--partition-guid', required=True)
p.add_argument('--flash-log', type=Path, required=True)
a = p.parse_args()
if a.output.exists():
    raise SystemExit('BCD出力ファイルが既にあります')
backup = a.backup.read_bytes()
if len(backup) != 65536 or any(backup):
    raise SystemExit('scratchバックアップが64 KiBのゼロデータではありません')
disk_guid = uuid.UUID(a.disk_guid).bytes_le
part_guid = uuid.UUID(a.partition_guid).bytes_le

def adb(*args):
    return subprocess.check_output(['adb', *args], text=True).strip()

if (adb('get-state') != 'recovery' or adb('shell', 'id', '-u') != '0' or
        adb('shell', 'readlink', '/dev/block/by-name/Windows') != '/dev/block/mmcblk0p20' or
        adb('shell', 'cat', '/sys/class/block/mmcblk0p20/start') != '917504' or
        adb('shell', 'cat', '/sys/class/block/mmcblk0p20/size') != '29584384'):
    raise SystemExit('root recoveryまたはWindows領域の位置が一致しません')
device_id = adb('shell', 'cat', '/sys/class/block/mmcblk0p20/dev')
if any(len(fields) > 2 and fields[2] == device_id
       for fields in (line.split() for line in adb('shell', 'cat', '/proc/self/mountinfo').splitlines())):
    raise SystemExit('Windows領域がマウントされています')
error = None
try:
    raw = subprocess.check_output(['adb', 'exec-out',
        'dd if=/dev/block/by-name/Windows bs=512 skip=2048 count=128 2>/dev/null'])
    if len(raw) != 65536 or raw[:8] != b'A04BCD1\0':
        raise ValueError('scratchヘッダーまたはサイズが不正です')
    size, crc = struct.unpack_from('<II', raw, 8)
    if not 0 < size <= 65024 or zlib.crc32(raw[512:512+size]) != crc:
        raise ValueError('BCDサイズまたはCRCが不正です')
    data = raw[512:512+size]
    if data[:4] != b'regf' or disk_guid not in data or part_guid not in data:
        raise ValueError('BCD署名または指定したGPT GUIDが見つかりません')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    with a.output.open('xb') as f:
        f.write(data)
    print('BCD SHA-256:', hashlib.sha256(data).hexdigest())
except Exception as exc:
    error = exc
finally:
    a.flash_log.parent.mkdir(parents=True, exist_ok=True)
    remote = '/tmp/a04br3-bcd-scratch-backup.bin'
    with a.flash_log.open('ab') as log:
        subprocess.run(['adb', 'push', str(a.backup), remote], stdout=log, stderr=log, check=True)
        remote_hash = adb('shell', 'sha256sum', remote).split()[0]
        if remote_hash != hashlib.sha256(backup).hexdigest():
            raise SystemExit('転送したscratchバックアップのハッシュが一致しません')
        subprocess.run(['adb', 'shell', 'dd', f'if={remote}', 'of=/dev/block/by-name/Windows',
                        'bs=512', 'seek=2048', 'count=128', 'conv=notrunc,fsync'],
                       stdout=log, stderr=log, check=True)
    print('scratchをバックアップから復元しました')
if error:
    raise SystemExit(f'BCD回収失敗: {error}')
