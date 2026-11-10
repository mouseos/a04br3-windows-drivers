#!/usr/bin/env python3
"""MT6630単独の一回更新。CRC付き保守結果を確認して通常ESPへ戻す。"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import time

SERIAL = '0C0U265083'
NORMAL = 'port/xnu/out/external-tab-a04br3-import/windows-rt/mma8652-hid/recovery-esp-r4.img'
NORMAL_SHA = '67ac0145d4917e1d53bfed15aff88a1585cf88ac73648733178e11ca3e64534a'
UART_DEVICE = '/dev/serial/by-id/usb-FTDI_FT232R_USB_UART_A50285BI-if00-port0'


def sha(p):
    with p.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for n in ('workspace', 'media', 'backup', 'out'):
        ap.add_argument('--' + n, type=Path, required=True)
    ap.add_argument('--manifest-sha256', required=True)
    a = ap.parse_args()
    root = a.workspace.resolve(strict=True)
    media, backup, out = (p.resolve() for p in (a.media, a.backup, a.out))
    if any(not p.is_relative_to(root) for p in (media, backup, out)) or out.exists():
        raise SystemExit('ルート内の入力と新規出力先が必要です')
    if not re.fullmatch('[0-9a-f]{64}', a.manifest_sha256) or sha(media / 'manifest.json') != a.manifest_sha256:
        raise SystemExit('媒体manifestが一致しません')
    m = json.loads((media / 'manifest.json').read_text())
    if sha(backup) != m['host_backup_manifest_sha256'] or not re.fullmatch('[0-9A-F]{16}', m['token']):
        raise SystemExit('今回のbackup/tokenが一致しません')
    if not re.fullmatch('[0-9a-f]{64}', m['esp_sha256']) or sha(media / 'update-esp.img') != m['esp_sha256'] or sha(root / NORMAL) != NORMAL_SHA:
        raise SystemExit('ESPの入力hashが一致しません')
    if (media / 'update-esp.img').stat().st_size != 268435456:
        raise SystemExit('ESPサイズが一致しません')
    out.mkdir(mode=0o700)
    uart = root / 'port/gsi/logs/ios-feasibility/windows-rt-uart-workspace.log'
    state = dict(status='started', media_manifest_sha256=a.manifest_sha256,
                 boot_written=False, gpt_written=False, normal_windows_preserved=False)

    def save(status):
        state['status'] = status
        (out / 'result.json').write_text(json.dumps(state, ensure_ascii=False, indent=2) + '\n')

    def run(argv, name, timeout=180, flash=False):
        with (out / name).open('ab') as log:
            r = subprocess.run(argv, cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=timeout,
                               env=dict(os.environ, UART=UART_DEVICE))
        if flash:
            with (root / 'port/gsi/logs/flash.log').open('ab') as f:
                f.write((out / name).read_bytes())
        r.check_returncode()

    def adb(*args):
        return ['adb', '-s', SERIAL, *args]

    def deploy(image, digest, label):
        # Verify only the RAM staging file. Never read back the physical ESP.
        run(adb('push', str(image), '/tmp/mt6630-verified-esp.img'), label + '.log')
        command = ('set -e; test "$(id -u)" = 0; '
                   'test "$(readlink -f /dev/block/by-name/EFI)" = /dev/block/mmcblk0p18; '
                   'test "$(cat /sys/class/block/mmcblk0p18/start)" = 360448; '
                   'test "$(cat /sys/class/block/mmcblk0p18/size)" = 524288; '
                   'awk \'NF < 6 {exit 2} $3 ~ /ntfs|fuseblk/ {exit 1} '
                   '$1 == "/dev/block/mmcblk0p18" || $1 == "/dev/block/by-name/EFI" {exit 1}\' /proc/mounts; '
                   'awk \'NF < 6 {exit 2} $3 == "179:18" || $3 == "179:20" {exit 1}\' /proc/self/mountinfo; '
                   'test "$(sha256sum /tmp/mt6630-verified-esp.img | cut -d " " -f 1)" = ' + digest + '; '
                   'dd if=/tmp/mt6630-verified-esp.img of=/dev/block/by-name/EFI bs=1048576 conv=fsync; sync')
        run(adb('shell', command), label + '-write.log', flash=True)
        offset = uart.stat().st_size
        run(adb('reboot'), label + '-reboot.log', timeout=30)
        return offset

    save('checking-same-recovery-backup')
    capture = Path(__file__).with_name('capture-windows-prebackup.py')
    run(['python3', '-B', str(capture), 'verify', '--serial', SERIAL, '--manifest', str(backup),
         '--manifest-sha256', m['host_backup_manifest_sha256']], 'backup-verify.log')
    offset = deploy(media / 'update-esp.img', m['esp_sha256'], 'maintenance')
    state['maintenance_uart_offset'] = offset
    save('offline-servicing-observing')
    spec = importlib.util.spec_from_file_location('audit', Path(__file__).with_name('decode-audit-stream.py'))
    decoder = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(decoder)
    deadline = time.monotonic() + 900
    report = None
    with uart.open('rb') as stream:
        stream.seek(offset)
        data = bytearray()
        while time.monotonic() < deadline:
            data.extend(stream.read())
            if re.search(rb'(?m)^A04AU2E ' + m['token'].encode() + rb' [0-9A-F]{8}\r?\n', data):
                report = decoder.decode(bytes(data), m['token'])
                break
            time.sleep(1)
    (out / 'maintenance-uart.log').write_bytes(data)
    if report is None:
        save('servicing-result-missing-do-not-reset')
        raise SystemExit('更新結果不明。処理中の可能性があるためresetしません')
    (out / 'maintenance-report.txt').write_bytes(report)
    lines = report.decode('ascii').splitlines()
    if any(lines.count(x) != 1 for x in ('UPDATE_STATUS=COMMITTED', 'UPDATE_RELEASE_EXIT=0', 'UPDATE_EXIT=0')):
        save('servicing-failed-no-normal-boot')
        raise SystemExit('更新またはvolume release失敗。通常Windowsへ進めません')
    state['offline_dism_committed'] = True
    save('servicing-committed-volume-released')
    run(['bash', str(root / 'unixnu/UniXNU-a04br3/scripts/a04br3-reset.sh')], 'maintenance-reset.log', timeout=30)
    deadline = time.monotonic() + 90
    while time.monotonic() < deadline:
        try:
            r = subprocess.run(['fastboot', '-s', SERIAL, 'getvar', 'product'], capture_output=True, timeout=5)
        except subprocess.TimeoutExpired:
            continue
        if r.returncode == 0 and re.search(rb'product:\s*A04BR3', r.stdout + r.stderr):
            break
        time.sleep(1)
    else:
        save('released-waiting-fastboot')
        raise SystemExit('fastboot復帰を確認できません')
    run(['fastboot', '-s', SERIAL, 'oem', 'reboot-recovery'], 'normal-recovery.log', timeout=30)
    run(adb('wait-for-recovery'), 'normal-recovery-wait.log', timeout=150)
    # Recovery root can restart adbd; wait-for-recovery does not prove root.
    run(adb('root'), 'normal-root.log', timeout=30)
    run(adb('wait-for-recovery'), 'normal-root-wait.log', timeout=60)
    state['normal_uart_offset'] = deploy(root / NORMAL, NORMAL_SHA, 'normal')
    state['normal_windows_preserved'] = True
    state['connection_verified'] = False
    state['loaded_driver_version_verified'] = False
    save('offline-update-committed-normal-boot-requested-hold')
    print('オフライン更新完了・通常Windows起動要求済み。接続と新ドライバ開始は未確認。')


if __name__ == '__main__':
    main()
