#!/usr/bin/env python3
"""実239を起点に更新なしWinPE資格確認を一度実行し、実241をRO回収する。"""
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

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
TRANSPORT_SECONDS = 2400
MIN_RESET_HOST_SECONDS = 1790
EXPIRED = b'A04RGU WinPE lease expired; SWRST requested'
MOUNT_GUARD = '''set -e
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
'''


def need(ok, message):
    if not ok:
        raise ValueError(message)


def sha(p):
    with p.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(p):
    return json.loads(p.read_text(encoding='utf-8-sig'))


def inside(root, p, existing=True):
    q = (root / p).resolve(strict=existing)
    need(q.is_relative_to(root), 'root外のpathは使用できません')
    return q


def load(name, p):
    spec = importlib.util.spec_from_file_location(name, p)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def complete(m):
    return (m.get('status') == 'captured-unmounted' and m.get('read_only') is True
            and m.get('unmounted') is True and m.get('errors') == [])


def file_rows(m):
    rows = {}
    for row in m['files']:
        name = row['source_path']
        need(isinstance(name, str) and name not in rows and not name.startswith('/')
             and '..' not in name.split('/') and '\\' not in name
             and type(row['bytes']) is int and row['bytes'] >= 0
             and re.fullmatch('[0-9a-f]{64}', row['sha256'] or ''), 'source file metadata不一致/重複')
        rows[name] = (row['bytes'], row['sha256'])
    return rows


def missing_rows(m):
    names = [r['source_path'] for r in m.get('missing_files', [])]
    need(len(names) == len(set(names)) and not set(names).intersection(file_rows(m)),
         'missing/present file重複')
    return set(names)


def validate_bundle(root, primary, supplement, usb, pins):
    m = read(primary)
    need(complete(m) and m.get('schema_version') == 1
         and re.fullmatch(r'[A-Za-z0-9_-]{1,100}', m.get('serial', ''))
         and re.fullmatch(r'[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}', m.get('recovery_boot_id', '')),
         '同bootの完了primary captureが必要です')
    need(len(file_rows(m)) == 42, '対象Windows全42filesが必要です')
    for name in ('SYSTEM', 'SYSTEM.LOG1', 'SYSTEM.LOG2'):
        need('Windows/System32/config/' + name in file_rows(m), 'SYSTEM trio欠落')
    result = []
    for path in (primary, supplement, usb):
        v = read(path)
        need(complete(v), '完了RO capture/補足が必要です')
        if path != primary:
            need(v['source_manifest_sha256'] == sha(primary)
                 and v['recovery_boot_id'] == m['recovery_boot_id'], '補足の同primary/boot結合不一致')
        file_rows(v)
        missing_rows(v)
        for row in v['files']:
            need(row['backup_path'] == 'files/' + row['source_path'], '固定backup path不一致')
            p = inside(root, path.parent / row['backup_path'])
            need(p.is_relative_to(path.parent) and p.is_file()
                 and p.stat().st_size == row['bytes'] and sha(p) == row['sha256'],
                 '回収fileの実path/bytes/hash不一致')
            pins[p] = sha(p)
        pins[path] = sha(path)
        result.append(v)
    return result


def evaluate_uart(raw, token, decoder, storage_trace):
    """host経過時間をkernel期限の測定値にしない。CRCと期限event順序を独立評価する。"""
    evidence = dict(runtime_query_valid=False, expired_then_boot4=False,
                    kernel_elapsed_low=None, kernel_elapsed_high=None,
                    kernel_duration_measured=False, qualification_transport_evidence=False)
    clean = re.sub(storage_trace, b'', raw)
    report = None
    try:
        need(re.fullmatch('[0-9A-F]{16}', token or ''), '固定CRC tokenが必要です')
        report = decoder.decode(clean, token)
        lines = report.decode().splitlines()
        expected = dict(LEASE_QUERY_EXIT='0', LEASE_EXPECTED_SECONDS='1800',
                        NORMAL_WINDOWS_UPDATE_REQUESTED='0')
        observed = {key: [] for key in expected}
        for line in lines:
            key, separator, value = line.partition('=')
            if key.strip() in observed:
                observed[key.strip()].append((key, separator, value))
        evidence['runtime_query_valid'] = all(
            observed[key] == [(key, '=', value)] for key, value in expected.items())
        ends = list(re.finditer(rb'A04AU2E ' + token.encode() + rb' [0-9A-F]{8}\r?\n', clean))
        need(len(ends) == 1, '同tokenのCRC終端は一度だけ必要です')
        end = ends[0].end()
        marker = b'A04RGU_WINPE_LEASE_V1:1800'
        need(marker in clean[:ends[0].start()], '同試験CRC前のlease markerが必要です')
        # 最初の後続bootより前にexpired行が必要。他のbootを飛ばしてBOOT4を選ばない。
        boots = list(re.finditer(rb'BOOT_REASON:\s*([0-9]+)(?:\D|$)', clean[end:]))
        need(boots, 'CRC後のboot境界がありません')
        first = boots[0]
        expired = list(re.finditer(rb'(?:^|\n)' + re.escape(EXPIRED) + rb'\r?\n',
                                   clean[end:end + first.start()]))
        evidence['expired_then_boot4'] = len(expired) == 1 and first[1] == b'4'
        evidence['qualification_transport_evidence'] = (
            evidence['runtime_query_valid'] and evidence['expired_then_boot4'])
    except Exception as exc:
        evidence['error'] = str(exc)
    return evidence, report


def preflight(a):
    root = a.workspace.resolve(strict=True)
    need(root == HERE.parents[3] and root.name == 'TAB-A04-BR3', '正本workspace rootが必要です')
    pins = {}

    def pin(p, digest=None):
        p = inside(root, p)
        need(p.is_file(), '実fileが必要です')
        h = sha(p)
        need(digest is None or (re.fullmatch('[0-9a-f]{64}', digest or '') and h == digest),
             '明示manifest/file pin不一致')
        pins[p] = h
        return p

    primary = pin(a.prior_manifest, a.prior_sha256)
    supplement = pin(a.prior_supplement, a.prior_supplement_sha256)
    usb = pin(a.prior_usb, a.prior_usb_sha256)
    prefix = root / 'port/xnu/out/private-windows-rt-prebackup-run239'
    need(primary == prefix / 'manifest.json'
         and supplement == prefix.with_name(prefix.name + '-gpu-assets') / 'manifest.json'
         and usb == prefix.with_name(prefix.name + '-usb1121-exe') / 'manifest.json',
         '実239の固定namespaceが必要です。改名流用はできません')
    before = validate_bundle(root, primary, supplement, usb, pins)
    qm = pin(a.qualification_manifest, a.qualification_sha256)
    q = read(qm)
    need(q['schema'] == 'WINPE_UNATTENDED_LEASE_QUALIFICATION_V1'
         and q['offline_windows_update_requested'] is False and q['boot_write_required'] is False
         and q['gpt_write_required'] is False and q['expected_reset_seconds'] == 1800
         and q['expected_reset_requires_no_UART_RX'] is True,
         '固定の更新なし1800秒資格確認媒体が必要です')
    esp = pin(qm.parent / 'qualification-esp.img', q['esp_sha256'])
    pin(qm.parent / 'qualification.wim', q['wim_sha256'])
    lp = pin(a.lease_manifest, a.lease_sha256)
    ap = pin(a.atomic_manifest, a.atomic_sha256)
    lease_tool = pin(HERE.parent / 'winpe/winpe_lease_package.py')
    lease = load('next_lease_package', lease_tool)
    need(callable(getattr(lease, 'atomic_signature_gate', None)), '独立署名gateの統合が必要です')
    lm, leaves = lease.load_package(root, lp, a.lease_sha256)
    am, exe = lease.load_atomic_helper(root, ap, a.atomic_sha256)
    for p in leaves.values():
        pin(p)
    pin(exe)
    # Turingの独立署名gateが照合した実証拠も待機後まで固定する。
    cert = am['signing']['public_certificate']
    pin(ap.parent / cert['name'], cert['sha256'])
    steps = {s['name']: s for s in am['steps']}
    for stage in ('sign-production', 'verify-production'):
        pin(ap.parent / 'logs' / (stage + '.log'), steps[stage]['log_sha256'])
    need(q['winpe_lease']['schema'] == 'WINPE_LEASE_EMBEDDED_V1'
         and q['winpe_lease']['manifest_path'] == lp.relative_to(root).as_posix()
         and q['winpe_lease']['manifest_sha256'] == a.lease_sha256
         and q['winpe_lease']['SYS_sha256'] == sha(leaves['mt8167-toprgu.sys'])
         and q['winpe_lease']['source_commit'] == lm['source_commit']
         and q['winpe_lease']['lease_seconds'] == 1800 and q['winpe_lease']['force_swrst'] is True
         and q['winpe_lease']['normal_windows_modified'] is False
         and q['atomic_manifest_sha256'] == a.atomic_sha256
         and q['atomic_EXE_sha256'] == sha(exe)
         and q['UART_token'] == a.lease_sha256[:16].upper(), '媒体/packageの実byte結合不一致')
    capture = pin(HERE / 'capture-windows-prebackup.py')
    extra = pin(HERE / 'capture-peripheral-preservation.py')
    usb_tool = pin(HERE / 'capture-usb1121-existing-exe.py')
    engine = pin(HERE / 'run-gpu368-fresh200.py')
    decoder = pin(root / 'windows-rt/a04br3-windows-public-transport/tools/recovery/decode-audit-stream.py')
    ntfs = pin(root / 'port/xnu/out/windows-rt-recovery-ntfs3g/ntfs-3g-aarch64-static')
    pin(Path(__file__).resolve())
    out = inside(root, a.out, False)
    need(out.is_relative_to(root / 'port/gsi/logs/ios-feasibility') and not out.exists(),
         'root内logsの新規outputが必要です')
    fresh = root / 'port/xnu/out/private-windows-rt-prebackup-run241'
    outputs = (fresh, fresh.with_name(fresh.name + '-gpu-assets'),
               fresh.with_name(fresh.name + '-usb1121-exe'))
    need(all(not p.exists() for p in outputs), '241 namespaceは再利用できません')
    return dict(root=root, pins=pins, q=q, qm=qm, esp=esp, primary=primary, before=before,
                capture=capture, extra=extra, usb=usb_tool, engine=engine, decoder=decoder,
                ntfs=ntfs, out=out, outputs=outputs, serial=before[0]['serial'])


def execute(a, c):
    root, out, serial = c['root'], c['out'], c['serial']
    lockpath = root / ('port/gsi/logs/ios-feasibility/unattended-qualification-' + serial + '.lock')
    with lockpath.open('a+') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        out.mkdir(mode=0o700, parents=True)
        return locked_execute(a, c)


def locked_execute(a, c):
    root, out, serial = c['root'], c['out'], c['serial']
    fresh, supplement, usb_out = c['outputs']
    state = dict(schema='UNATTENDED_WINPE_QUALIFICATION_NEXT_V1', status='preflight',
                 prior_capture=239, post_capture=241, hardware_verified=False, GPU_success=False,
                 qualification_pass=False, evidence_recovered=False, boot_written=False,
                 gpt_written=False, esp_write_attempted=False, esp_written=False,
                 UART_reset_requested=False, normal_windows_update_requested=False,
                 qualification_manifest_sha256=a.qualification_sha256,
                 prior_manifest_sha256=a.prior_sha256, transport_wait_seconds=TRANSPORT_SECONDS,
                 kernel_duration_measured=False,
                 pins=[dict(path=p.relative_to(root).as_posix(), sha256=h) for p, h in c['pins'].items()])

    def save():
        (out / 'result.json').write_text(json.dumps(state, ensure_ascii=False, indent=2) + '\n')

    def unchanged():
        need(all(sha(p) == h for p, h in c['pins'].items()), '試験入力が変更されました')

    def free_outputs():
        need(all(not p.exists() for p in c['outputs']), '241 outputが既に使用されています')

    def command(args, name, timeout=30, check=True):
        try:
            p = subprocess.run(args, cwd=root, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, timeout=timeout)
        except subprocess.TimeoutExpired as exc:
            (out / name).write_bytes(exc.output or b'')
            raise
        (out / name).write_bytes(p.stdout)
        if check:
            p.check_returncode()
        return p

    def adb(*args, **kw):
        return command(['adb', '-s', serial, *args], **kw)

    save()
    try:
        unchanged()
        free_outputs()
        command([sys.executable, '-B', str(c['capture']), 'verify', '--serial', serial,
                 '--manifest', str(c['primary']), '--manifest-sha256', a.prior_sha256],
                'verify239.log', 300)
        backend = load('next_capture_backend', c['capture'])
        recovery = backend.Recovery(serial)
        identity = recovery.identity()
        recovery.require_unmounted(identity)
        need(all(c['before'][0].get(k) == v for k, v in identity.items()),
             '239取得時と同じRecovery boot/serial/geometryが必要です')
        once_id = hashlib.sha256((a.qualification_sha256 + a.prior_sha256).encode()).hexdigest()
        latch = root / ('port/gsi/logs/ios-feasibility/qualification-once-' + once_id + '.json')
        with latch.open('x') as f:
            json.dump(dict(schema='QUALIFICATION_239_TO241_ONCE_V1', output=out.relative_to(root).as_posix(),
                           qualification_sha256=a.qualification_sha256, prior_sha256=a.prior_sha256), f)
            f.flush()
            os.fsync(f.fileno())
        directory = os.open(latch.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)
        state['once_latch'] = latch.relative_to(root).as_posix()
        save()
        adb('push', str(c['esp']), '/tmp/lease-qualification-esp.img', name='push-esp.log', timeout=180)
        p = adb('shell', 'sha256sum /tmp/lease-qualification-esp.img', name='staged-esp-sha.log')
        need(p.stdout.split()[0].decode() == c['q']['esp_sha256'], '転送ESP hash不一致')
        unchanged()
        free_outputs()
        recovery.require_unmounted(identity)
        state.update(status='esp-write-starting', esp_write_attempted=True)
        save()
        try:
            adb('shell', MOUNT_GUARD + '\ndd if=/tmp/lease-qualification-esp.img of=/dev/block/mmcblk0p18 bs=4M conv=fsync\nsync\nrm /tmp/lease-qualification-esp.img\n',
                name='write-esp.log', timeout=180)
        finally:
            if (out / 'write-esp.log').exists():
                with (root / 'port/gsi/logs/flash.log').open('ab') as f:
                    f.write((out / 'write-esp.log').read_bytes())
        state.update(esp_written=True, status='waiting-for-independent-reset')
        uart = inside(root, 'port/gsi/logs/ios-feasibility/windows-rt-uart-workspace.log')
        offset = uart.stat().st_size
        began = time.monotonic()
        state.update(UART_offset=offset, qualification_boot_requested=True,
                     host_reboot_requested_monotonic=began)
        save()
        try:
            adb('reboot', name='boot-qualification.log')
        except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as exc:
            state['qualification_boot_ack'] = type(exc).__name__ + '; observing without resending'
            save()
        deadline = began + TRANSPORT_SECONDS
        fastboot = False
        fastboot_observed = None
        while time.monotonic() < deadline:
            try:
                p = command(['fastboot', '-s', serial, 'getvar', 'product'], 'fastboot-probe.log',
                            min(5, max(.01, deadline - time.monotonic())), False)
                if p.returncode == 0:
                    need(re.search(rb'product:\s*A04BR3(?:\s|$)', p.stdout), 'fastboot product不一致')
                    fastboot_observed = time.monotonic()
                    fastboot = True
                    break
            except subprocess.TimeoutExpired:
                pass
            time.sleep(min(5, max(0, deadline - time.monotonic())))
        with uart.open('rb') as f:
            f.seek(offset)
            raw = f.read()
        (out / 'qualification-uart.log').write_bytes(raw)
        elapsed = (fastboot_observed if fastboot else time.monotonic()) - began
        state['host_transport_elapsed_seconds'] = elapsed
        # 早期resetを保守的に拒否するだけで、kernel期限の精度測定ではない。
        state['host_reset_window_valid'] = MIN_RESET_HOST_SECONDS <= elapsed <= TRANSPORT_SECONDS
        state['minimum_reset_host_seconds'] = MIN_RESET_HOST_SECONDS
        need(fastboot, '2400秒以内にfastboot productを確認できませんでした。再resetしません')
        decoder = load('next_qualification_crc', c['decoder'])
        engine = load('next_qualification_engine', c['engine'])
        proof, report = evaluate_uart(raw, c['q']['UART_token'], decoder, engine.STORAGE_TRACE)
        state['lease_evidence'] = proof
        if report is not None:
            (out / 'qualification-report.txt').write_bytes(report)
        state.update(status='reset-transport-observed-collecting-evidence', fastboot_identity_verified=True)
        save()
        # query/CRC不合格でも、reset後のRO証拠回収は独立に行う。要求は一度だけ。
        try:
            command(['fastboot', '-s', serial, 'oem', 'reboot-recovery'], 'to-recovery.log', 20)
        except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as exc:
            state['recovery_transition_ack'] = type(exc).__name__ + '; observing without resending'
            save()
        deadline = time.monotonic() + 150
        root_requested = False
        ready = False
        while time.monotonic() < deadline:
            try:
                p = adb('get-state', name='recovery-probe.log', timeout=min(5, max(.01, deadline-time.monotonic())), check=False)
                if p.returncode == 0 and p.stdout.strip() == b'recovery':
                    p = adb('get-serialno', name='serial.log', timeout=5)
                    need(p.stdout.strip().decode() == serial, 'Recovery serial不一致')
                    uid = adb('shell', 'id -u', name='uid.log', timeout=5).stdout.strip()
                    if uid == b'0':
                        ready = True
                        break
                    if not root_requested:
                        root_requested = True
                        state['root_requested'] = True
                        save()
                        adb('root', name='root.log', timeout=10)
            except (subprocess.TimeoutExpired, subprocess.CalledProcessError):
                pass
            time.sleep(min(2, max(0, deadline-time.monotonic())))
        need(ready, 'Recovery/root待機上限です。再送しません')
        unchanged()
        after = recovery.identity()
        recovery.require_unmounted(after)
        need(after['recovery_boot_id'] != identity['recovery_boot_id'], '新しいRecovery bootが必要です')
        free_outputs()
        adb('push', str(c['ntfs']), '/tmp/rt-ntfs3g', name='push-ntfs.log', timeout=120)
        adb('shell', 'chmod 755 /tmp/rt-ntfs3g', name='chmod.log')
        args = [sys.executable, '-B', str(c['capture']), 'capture', '--serial', serial, '--out', str(fresh)]
        for name in engine.TARGET_SYS:
            args += ['--target-sys', name + '.sys']
        args += ['--allow-missing-sys', '--engine-probe-state', '--sd-component-state',
                 '--allow-missing-engine-service-log', '--mount-driver', 'ntfs-3g',
                 '--ntfs3g-remote', '/tmp/rt-ntfs3g']
        command(args, 'capture241.log', 1800)
        mf = fresh / 'manifest.json'
        command([sys.executable, '-B', str(c['extra']), '--manifest', str(mf), '--manifest-sha256', sha(mf),
                 '--out', str(supplement), '--gpu-runtime-assets', '--include-system-dxgi'], 'assets241.log', 1800)
        command([sys.executable, '-B', str(c['usb']), '--manifest', str(mf), '--manifest-sha256', sha(mf),
                 '--out', str(usb_out), '--include-usb1121-existing-exe'], 'usb241.log', 1800)
        observed = {}
        bundle = validate_bundle(root, mf, supplement / 'manifest.json', usb_out / 'manifest.json', observed)
        need(all(bundle[0].get(k) == v for k, v in after.items()), '実241のRecovery identity不一致')
        need(recovery.identity() == after, '回収中にRecovery bootが変わりました')
        recovery.require_unmounted(after)
        unchanged()
        same = all(file_rows(before) == file_rows(post) and missing_rows(before) == missing_rows(post)
                   for before, post in zip(c['before'], bundle))
        passed = same and proof['qualification_transport_evidence'] and state['host_reset_window_valid']
        state.update(status='reset-recovery-and-capture241-complete' if passed else 'capture241-complete-qualification-failed',
                     evidence_recovered=True, qualification_pass=passed, hardware_verified=passed,
                     Windows_files_unchanged=same, capture_manifest=mf.relative_to(root).as_posix(),
                     capture_manifest_sha256=sha(mf), supplement_sha256=sha(supplement/'manifest.json'),
                     usb_witness_sha256=sha(usb_out/'manifest.json'))
        save()
        return 0 if passed else 2
    except Exception as exc:
        state.update(status='stopped-preserve-evidence', error=str(exc))
        save()
        raise


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('workspace', 'qualification-manifest', 'lease-manifest', 'atomic-manifest',
                 'prior-manifest', 'prior-supplement', 'prior-usb', 'out'):
        ap.add_argument('--' + name, type=Path, required=True)
    for name in ('qualification-sha256', 'lease-sha256', 'atomic-sha256', 'prior-sha256',
                 'prior-supplement-sha256', 'prior-usb-sha256'):
        ap.add_argument('--' + name, required=True)
    ap.add_argument('--validate-only', action='store_true')
    a = ap.parse_args(argv)
    c = preflight(a)
    if a.validate_only:
        print('QUALIFICATION_239_TO241_VALIDATED_NO_DEVICE_OPERATION')
        return 0
    return execute(a, c)


if __name__ == '__main__':
    sys.exit(main())
