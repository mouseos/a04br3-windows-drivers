#!/usr/bin/env python3
"""既回収待機の完了後、UART resetなしのWinPE期限復帰試験を一度だけ実行する。"""
import argparse
import fcntl
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
SERIAL = '0C0U265083'
sys.dont_write_bytecode = True


def need(value, message):
    if not value:
        raise ValueError(message)


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def read(p):
    return json.loads(p.read_text(encoding='utf-8-sig'))


def load(name, p):
    spec = importlib.util.spec_from_file_location(name, p)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('workspace', 'qualification-manifest', 'after-capture-wait', 'out'):
        ap.add_argument('--' + name, type=Path, required=True)
    ap.add_argument('--qualification-sha256', required=True)
    ap.add_argument('--after-seal-sha256', required=True)
    ap.add_argument('--watcher-pid', type=int, required=True)
    ap.add_argument('--validate-only', action='store_true')
    a = ap.parse_args()
    root = a.workspace.resolve(strict=True)

    def inside(p, exists=True):
        q = (root / p).resolve(strict=exists)
        need(q.is_relative_to(root), 'root外のpathは使用できません')
        return q

    manifest = inside(a.qualification_manifest)
    need(re.fullmatch('[0-9a-f]{64}', a.qualification_sha256 or '')
         and sha(manifest) == a.qualification_sha256, '資格確認manifest pin不一致')
    q = read(manifest)
    need(q['schema'] == 'WINPE_UNATTENDED_LEASE_QUALIFICATION_V1'
         and q['offline_windows_update_requested'] is False
         and q['boot_write_required'] is False and q['gpt_write_required'] is False
         and q['expected_reset_seconds'] == 1800, '固定の更新なし資格確認イメージだけを使用します')
    esp = inside(manifest.parent / 'qualification-esp.img')
    wim = inside(manifest.parent / 'qualification.wim')
    need(sha(esp) == q['esp_sha256'] and sha(wim) == q['wim_sha256'], '資格確認イメージ実byte不一致')
    wait = inside(a.after_capture_wait)
    old = read(wait / 'result.json')
    need(old['old_seal_sha256'] == a.after_seal_sha256 and old['next_capture_number'] == 237,
         '既run236後の回収待機だけを引き継ぎます')
    proc = Path('/proc') / str(a.watcher_pid)
    birth = None
    if old['status'] != 'new-capture-and-supplements-ready':
        birth = (proc / 'stat').read_text().split()[21]
        need(b'capture-after-clean-rejection.py' in (proc / 'cmdline').read_bytes(), '既watcherの実processが必要です')
    out = inside(a.out, False)
    need(not out.exists(), '新しい試験出力先が必要です')
    capture = HERE / 'capture-windows-prebackup.py'
    extra = HERE / 'capture-peripheral-preservation.py'
    usb = HERE / 'capture-usb1121-existing-exe.py'
    engine_path = HERE / 'run-gpu368-fresh200.py'
    engine = load('qualification_engine', engine_path)
    decoder_path = root / 'windows-rt/a04br3-windows-public-transport/tools/recovery/decode-audit-stream.py'
    decoder = load('qualification_crc', decoder_path)
    ntfs = root / 'port/xnu/out/windows-rt-recovery-ntfs3g/ntfs-3g-aarch64-static'
    pins = {p: sha(p) for p in (Path(__file__).resolve(), capture, extra, usb, engine_path,
                                decoder_path, ntfs, manifest, esp, wim)}
    fresh = root / 'port/xnu/out/private-windows-rt-prebackup-run239'
    supplement = fresh.with_name(fresh.name + '-gpu-assets')
    usb_out = fresh.with_name(fresh.name + '-usb1121-exe')
    need(all(not p.exists() for p in (fresh, supplement, usb_out)), '新しい239回収namespaceが必要です')
    if a.validate_only:
        print('UNATTENDED_QUALIFICATION_VALIDATED_NO_DEVICE_OPERATION')
        return
    transport_lock=(root/('port/gsi/logs/ios-feasibility/unattended-qualification-'+SERIAL+'.lock')).open('a+')
    fcntl.flock(transport_lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
    out.mkdir(parents=True, mode=0o700)
    state = dict(schema='UNATTENDED_WINPE_QUALIFICATION_RUNTIME_V1', status='waiting-for-capture237',
                 qualification_manifest_sha256=a.qualification_sha256, boot_written=False,
                 gpt_written=False, esp_written=False, esp_write_attempted=False,
                 UART_reset_requested=False, normal_windows_update_requested=False,
                 hardware_verified=False, GPU_success=False,
                 pins=[dict(path=p.relative_to(root).as_posix(), sha256=h) for p, h in pins.items()])

    def save():
        (out / 'result.json').write_text(json.dumps(state, ensure_ascii=False, indent=2) + '\n')

    def unchanged():
        need(all(sha(p) == h for p, h in pins.items()), '待機中に試験入力が変わりました')

    def command(args, name, timeout=30, check=True):
        try:
            p = subprocess.run(args, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
        except subprocess.TimeoutExpired as exc:
            (out/name).write_bytes(exc.output or b'')
            raise
        (out / name).write_bytes(p.stdout)
        if check:
            p.check_returncode()
        return p

    def adb(*args, name='adb.log', timeout=30):
        return command(['adb', '-s', SERIAL, *args], name, timeout)

    save()
    try:
        deadline = time.monotonic() + 7200
        while time.monotonic() < deadline:
            current = read(wait / 'result.json')
            need(current['old_seal_sha256'] == a.after_seal_sha256, '既回収のsealが変わりました')
            if current['status'] == 'new-capture-and-supplements-ready':
                break
            need(current['status'] in ('waiting-for-transport', 'capturing-new-generation'),
                 '既回収が停止しました。重複回収せず証拠を保持します')
            need((proc / 'stat').read_text().split()[21] == birth, '既watcher processが終了しました')
            time.sleep(30)
        else:
            raise ValueError('既回収待機の上限に到達しました')
        unchanged()
        need(all(not p.exists() for p in (fresh,supplement,usb_out)),'239 namespaceが既に使用されています')
        prior = inside(current['capture_manifest'])
        prior_sha = current['capture_manifest_sha256']
        need(sha(prior) == prior_sha, '実237 manifest hash不一致')
        command([sys.executable, '-B', str(capture), 'verify', '--serial', SERIAL,
                 '--manifest', str(prior), '--manifest-sha256', prior_sha], 'verify237.log', 300)
        backend = load('qualification_capture', capture)
        recovery = backend.Recovery(SERIAL)
        identity = recovery.identity()
        recovery.require_unmounted(identity)
        need(identity['recovery_boot_id'] == read(prior)['recovery_boot_id'], '237取得後にRecoveryが変わっています')
        uart = root / 'port/gsi/logs/ios-feasibility/windows-rt-uart-workspace.log'
        offset = uart.stat().st_size
        once_id=hashlib.sha256((a.qualification_sha256+a.after_seal_sha256+prior_sha).encode()).hexdigest()
        latch=root/('port/gsi/logs/ios-feasibility/qualification-once-'+once_id+'.json')
        with latch.open('x') as f:
            json.dump(dict(qualification_sha256=a.qualification_sha256,prior_manifest_sha256=prior_sha,output=str(out.relative_to(root))),f)
            f.flush();os.fsync(f.fileno())
        directory=os.open(latch.parent,os.O_RDONLY|os.O_DIRECTORY)
        try:os.fsync(directory)
        finally:os.close(directory)
        state['once_latch']=str(latch.relative_to(root));save()
        adb('push', str(esp), '/tmp/lease-qualification-esp.img', name='push-esp.log', timeout=180)
        actual = adb('shell', 'sha256sum /tmp/lease-qualification-esp.img', name='staged-esp-sha.log').stdout.split()[0].decode()
        need(actual == q['esp_sha256'], 'Recoveryへ転送したESPのhash不一致')
        recovery.require_unmounted(identity)
        need(all(not p.exists() for p in (fresh,supplement,usb_out)),'239 namespaceが書込み前に使用されました')
        state.update(status='qualification-esp-write-starting', esp_write_attempted=True,
                     prior_manifest_sha256=prior_sha, UART_offset=offset)
        save()
        script = '''set -e
test "$(id -u)" = 0
test "$(readlink -f /dev/block/by-name/EFI)" = /dev/block/mmcblk0p18
test "$(cat /sys/class/block/mmcblk0p18/start)" = 360448
test "$(cat /sys/class/block/mmcblk0p18/size)" = 524288
rows=0
while read -r dev mp fs opts dump pass; do
 rows=$((rows+1)); test -n "$pass"
 case "$fs" in *ntfs*|*fuseblk*) exit 7;; esac
 case "$dev" in /dev/block/mmcblk0p17|/dev/block/mmcblk0p18|/dev/block/by-name/boot|/dev/block/by-name/EFI) exit 7;; esac
done < /proc/mounts
test "$rows" -gt 0
rows=0
while read -r mid parent major rest; do
 rows=$((rows+1)); test -n "$rest"
 case "$major" in 179:17|179:18|179:20) exit 7;; esac
done < /proc/self/mountinfo
test "$rows" -gt 0
dd if=/tmp/lease-qualification-esp.img of=/dev/block/mmcblk0p18 bs=4M conv=fsync
sync
rm /tmp/lease-qualification-esp.img
'''
        try:
            adb('shell', script, name='write-esp.log', timeout=180)
        finally:
            if (out/'write-esp.log').exists():
                with (root/'port/gsi/logs/flash.log').open('ab') as log:log.write((out/'write-esp.log').read_bytes())
        state.update(status='awaiting-independent-kernel-reset', esp_written=True)
        save()
        began = time.monotonic()
        state['qualification_boot_requested']=True;save()
        try:
            adb('reboot', name='boot-qualification.log')
        except (subprocess.TimeoutExpired,subprocess.CalledProcessError) as exc:
            state['qualification_boot_ack']=type(exc).__name__+'; observing without resending';save()
        deadline = began + 1900
        fastboot = False
        while time.monotonic() < deadline:
            try:
                p = command(['fastboot', '-s', SERIAL, 'getvar', 'product'], 'fastboot-probe.log', 5, False)
                if p.returncode == 0 and re.search(rb'product:\s*A04BR3(?:\s|$)', p.stdout):
                    fastboot = True
                    break
            except subprocess.TimeoutExpired:
                pass
            time.sleep(5)
        with uart.open('rb') as f:
            f.seek(offset)
            raw = f.read()
        (out / 'qualification-uart.log').write_bytes(raw)
        need(fastboot, '1900秒以内の独立リセット復帰を確認できませんでした')
        report = decoder.decode(re.sub(engine.STORAGE_TRACE, b'', raw), q['UART_token'])
        (out / 'qualification-report.txt').write_bytes(report)
        lines = report.decode().splitlines()
        need(all(lines.count(v) == 1 for v in ('LEASE_QUERY_EXIT=0', 'LEASE_EXPECTED_SECONDS=1800',
                                               'NORMAL_WINDOWS_UPDATE_REQUESTED=0'))
             and b'A04RGU_WINPE_LEASE_V1:1800' in raw,
             '実leaseの起動・query・更新なし報告が揃っていません')
        elapsed=time.monotonic()-began
        ends=list(re.finditer(rb'A04AU2E '+q['UART_token'].encode()+rb' [0-9A-F]{8}[^\n]*\n',raw))
        need(ends and 1790<=elapsed<=1900
             and re.search(rb'BOOT_REASON:\s*4(?:\D|$)',raw[ends[-1].end():]),
             '期限近傍の新boot境界とSWRST起動理由4が必要です')
        state.update(status='independent-reset-observed', elapsed_seconds=elapsed,
                     fastboot_identity_verified=True)
        save()
        # 一度の遷移要求。ack timeoutを理由に再送しない。
        try:
            command(['fastboot', '-s', SERIAL, 'oem', 'reboot-recovery'], 'to-recovery.log', 20)
        except (subprocess.TimeoutExpired, subprocess.CalledProcessError):
            pass
        deadline = time.monotonic() + 150
        root_requested = False
        while time.monotonic() < deadline:
            try:
                p = command(['adb', '-s', SERIAL, 'get-state'], 'recovery-probe.log', 5, False)
                if p.returncode == 0 and p.stdout.strip() == b'recovery':
                    uid = adb('shell', 'id -u', name='uid.log', timeout=5).stdout.strip()
                    if uid == b'0':
                        break
                    if not root_requested:
                        root_requested = True
                        adb('root', name='root.log', timeout=10)
            except (subprocess.TimeoutExpired, subprocess.CalledProcessError):
                pass
            time.sleep(2)
        else:
            raise ValueError('期限リセット後にRecovery/rootへ戻りませんでした')
        unchanged()
        after = recovery.identity()
        recovery.require_unmounted(after)
        need(after['recovery_boot_id'] != identity['recovery_boot_id'], '新しいRecovery bootが必要です')
        adb('push', str(ntfs), '/tmp/rt-ntfs3g', name='push-ntfs.log', timeout=120)
        adb('shell', 'chmod 755 /tmp/rt-ntfs3g', name='chmod.log')
        args = [sys.executable, '-B', str(capture), 'capture', '--serial', SERIAL, '--out', str(fresh)]
        for name in engine.TARGET_SYS:
            args += ['--target-sys', name + '.sys']
        args += ['--allow-missing-sys', '--engine-probe-state', '--sd-component-state',
                 '--allow-missing-engine-service-log', '--mount-driver', 'ntfs-3g',
                 '--ntfs3g-remote', '/tmp/rt-ntfs3g']
        command(args, 'capture239.log', 1800)
        mf = fresh / 'manifest.json'
        m = read(mf)
        need(m['status'] == 'captured-unmounted' and m['read_only'] is True and m['unmounted'] is True
             and m['errors'] == [] and m['recovery_boot_id'] == after['recovery_boot_id'], '実239回収が未完了です')
        before_rows = {v['source_path']: v['sha256'] for v in read(prior)['files']}
        after_rows = {v['source_path']: v['sha256'] for v in m['files']}
        need(before_rows == after_rows, '資格確認前後の通常Windows file bytesが変わっています')
        command([sys.executable, '-B', str(extra), '--manifest', str(mf), '--manifest-sha256', sha(mf),
                 '--out', str(supplement), '--gpu-runtime-assets', '--include-system-dxgi'], 'assets239.log', 1800)
        command([sys.executable, '-B', str(usb), '--manifest', str(mf), '--manifest-sha256', sha(mf),
                 '--out', str(usb_out), '--include-usb1121-existing-exe'], 'usb239.log', 1800)
        for artifact in (supplement,usb_out):
            am=read(artifact/'manifest.json')
            need(am['status']=='captured-unmounted' and am['read_only'] is True
                 and am['unmounted'] is True and am['errors']==[]
                 and am['source_manifest_sha256']==sha(mf)
                 and am['recovery_boot_id']==after['recovery_boot_id'],'同bootの補足回収が未完了です')
        recovery.require_unmounted(after)
        state.update(status='reset-recovery-and-capture239-complete', hardware_verified=True,
                     capture_manifest=str(mf.relative_to(root)), capture_manifest_sha256=sha(mf),
                     Windows_files_unchanged=True, supplement_sha256=sha(supplement / 'manifest.json'),
                     usb_witness_sha256=sha(usb_out / 'manifest.json'))
        save()
    except Exception as exc:
        state.update(status='stopped-preserve-evidence', error=str(exc))
        save()
        raise


if __name__ == '__main__':
    main()
