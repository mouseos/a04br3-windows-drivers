#!/usr/bin/env python3
"""署名render engineを一度実行し、WinPE lease後のRecoveryから証拠を回収する。"""
import argparse
import fcntl
import hashlib
import importlib.util
import json
import re
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
WAIT_SECONDS = 1900
sys.dont_write_bytecode = True


def need(ok, message):
    if not ok:
        raise ValueError(message)


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def inside(root, path, existing=False):
    p = (root / path).resolve(strict=existing)
    need(p.is_relative_to(root), '作業root外のpathは使用できません')
    return p


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def completed_capture(m):
    return (m.get('status') == 'captured-unmounted' and m.get('read_only') is True
            and m.get('unmounted') is True and m.get('errors') == [])


def failure_eligible(result, seal, digest):
    return (result.get('transaction') == seal['transaction']
            and result.get('seal_sha256') == digest
            and result.get('status') == 'pipeline-stopped'
            and result.get('prior_status') in ('prepare-observing', 'prepared-volume-released')
            and result.get('boot_written') is False and result.get('gpt_written') is False
            and not result.get('boot_write_attempted', False))


def validate_atomic(root, media, pinned, lease_module):
    atomic = media['atomic_publication']
    need(atomic['schema'] == 'WINPE_ATOMIC_PUBLICATION_V1'
         and type(atomic['payload_copy_count']) is int and atomic['payload_copy_count'] >= 1
         and type(atomic['system_publication_count']) is int
         and atomic['system_publication_count'] == 1
         and atomic['deadline_query_required'] is True and atomic['hardware_verified'] is False,
         '期限照会必須・SYSTEM publication一回のatomic媒体が必要です')
    manifest = pinned(atomic['manifest_path'])
    need(sha(manifest) == atomic['manifest_sha256'], 'atomic manifest pin不一致')
    _, exe = lease_module.load_atomic_helper(root, manifest, atomic['manifest_sha256'])
    exe = pinned(exe)
    need(sha(exe) == atomic['EXE_sha256']
         and media['files']['atomic-stage-replace.exe'] == atomic['EXE_sha256'],
         'atomic実EXE/media内hash不一致')


def validate_collection(seal, policy, number, collection):
    expected = dict(schema='SIGNED_RENDER_UNATTENDED_COLLECTION_V1', capture=number,
                    bounded_winpe_required=True, require_atomic_publication=True)
    canonical = lambda value: json.dumps(value, sort_keys=True, separators=(',', ':'))
    need(type(number) is int and 237 <= number <= 999
         and canonical(policy.get('unattended_collection_reboot')) == canonical(expected),
         '実capture237以降の明示無人collection markerが必要です')
    need(canonical(collection.admit(seal, policy, True)) == canonical(collection.CONFIG),
         'seal/policyの既collection configが必要です')


def verify_collected(root, seal, digest, result):
    match = re.fullmatch(r'port/xnu/out/private-windows-rt-prebackup-run([0-9]+)',
                         seal['capture_output'])
    need(match is not None, '固定capture出力名が必要です')
    need(result.get('status') == 'capture%d-and-diagnostics-ready-for-main-review' % int(match[1])
         and result.get('transaction') == seal['transaction']
         and result.get('seal_sha256') == digest
         and result.get('boot_written') is False and result.get('gpt_written') is False,
         'engine rc0だけでは回収完了ではありません。held/未回収状態は拒否します')
    proof = result.get('collection_shutdown', {})
    need(proof.get('schema') == 'MT8167_COLLECTION_REBOOT_HOST_PROOF_V1'
         and proof.get('fastboot_identity_verified') is True
         and proof.get('UART_reset_requested') is False,
         '追加UART resetなしのcollection移行証拠が必要です')
    fresh = inside(root, seal['capture_output'], True)
    mf = inside(root, fresh / 'manifest.json', True)
    primary = read(mf)
    need(sha(mf) == result.get('capture_manifest_sha256') and completed_capture(primary)
         and primary.get('serial') == seal['serial'], '完了primary capture/hash不一致')
    diagnostics = inside(root, seal['diagnostics_output'], True)
    index_path = inside(root, diagnostics / 'capture-index.json', True)
    index = read(index_path)
    need(sha(index_path) == result.get('diagnostics_index_sha256')
         and index.get('windows_unmounted') is True, '診断回収index/hash/unmount不一致')
    for base, rows, key in ((fresh, primary['files'], 'backup_path'),
                            (diagnostics, index['files'], 'path')):
        need(isinstance(rows, list) and rows, '実回収file一覧が必要です')
        seen = set()
        for row in rows:
            leaf = inside(root, base / row[key], True)
            need(leaf.is_relative_to(base) and leaf.is_file() and leaf not in seen
                 and leaf.stat().st_size == row['bytes'] and sha(leaf) == row['sha256'],
                 '回収済みfileの実byte/出力path不一致')
            seen.add(leaf)
    return dict(capture_manifest_sha256=sha(mf), diagnostics_index_sha256=sha(index_path))


def validate(root, seal_path, digest, number, out):
    need(re.fullmatch('[0-9a-f]{64}', digest or '') and sha(seal_path) == digest,
         'seal SHA不一致')
    c = read(seal_path)
    pins = {}
    for row in c['pinned_files']:
        path = inside(root, row['path'], True)
        key = path.relative_to(root).as_posix()
        need(key == row['path'] and key not in pins
             and re.fullmatch('[0-9a-f]{64}', row['sha256'])
             and sha(path) == row['sha256'], 'seal pin不一致/重複')
        pins[key] = row['sha256']

    def pinned(path):
        p = inside(root, path, True)
        need(pins.get(p.relative_to(root).as_posix()) == sha(p), '必要pin欠落/不一致')
        return p

    engine_path = pinned(c['controller_engine'])
    need(engine_path == HERE / 'run-gpu368-fresh200.py'
         and pinned(c['controller_path']) == engine_path, '既engineだけを実行できます')
    engine = load('unattended_render_engine', engine_path)
    profile = 'render-update-%d' % number
    k = engine.contract(profile)
    need(out == inside(root, k['runtime_output']), '世代専用outputだけを使用できます')
    need(c['boot_write_required'] is False, 'boot書込みを伴うsealは使用できません')
    media = inside(root, c['media_directory'], True)
    mp = pinned(media / 'manifest.json')
    need(sha(mp) == c['media_manifest_sha256'], 'media manifest SHA不一致')
    m = read(mp)
    lease = m['winpe_lease']
    need(lease['schema'] == 'WINPE_LEASE_EMBEDDED_V1'
         and type(lease['lease_seconds']) is int and lease['lease_seconds'] == 1800
         and lease['force_swrst'] is True and lease['normal_windows_modified'] is False,
         '1800秒強制SWRST付きWinPE専用leaseが必要です')
    if 'winpe_lease' in c:
        need(c['winpe_lease'] == lease, 'seal/mediaのleaseが不一致です')
    package_path = pinned(lease['manifest_path'])
    need(sha(package_path) == lease['manifest_sha256'], 'lease package manifest pin不一致')
    pinned(Path(__file__).resolve())
    lease_tool = pinned(HERE.parent / 'winpe/winpe_lease_package.py')
    lease_module = load('unattended_winpe_lease', lease_tool)
    package, files = lease_module.load_package(root, package_path, lease['manifest_sha256'])
    for leaf in files.values():
        pinned(leaf)
    need(lease['SYS_sha256'] == sha(files['mt8167-toprgu.sys'])
         and lease['source_commit'] == package['source_commit'], 'lease実SYS/source不一致')
    if 'atomic_publication' in c:
        need(c['atomic_publication'] == m['atomic_publication'], 'seal/mediaのatomic記載が不一致です')
    validate_atomic(root, m, pinned, lease_module)
    collection = load('unattended_collection_contract', pinned(HERE / 'collection_reboot.py'))
    validate_collection(c, m['policy'], number, collection)
    # load_packageは署名報告と実leafを検証するが、既engineのadmissionを代替しない。
    engine.validate_seal(root, seal_path, digest, profile)
    fresh = inside(root, c['capture_output'])
    need(c['capture_output'] == k['capture_output'], 'capture世代の改名流用は禁止です')
    supp = inside(root, fresh.with_name(fresh.name + '-gpu-assets'))
    need(not out.exists() and all(not p.exists() for p in (fresh, supp)),
         '既存runtime/capture/補足出力は再利用できません')
    capture = pinned(c['capture_tool'])
    need(capture == HERE / 'capture-windows-prebackup.py', '既capture helperのみ使用できます')
    extra = inside(root, HERE / 'capture-peripheral-preservation.py', True)
    ntfs = pinned(c['ntfs3g'])
    # 新wrapper/補足/lease loaderも開始時byteを固定し、移行後に再検証する。
    startup = {p: sha(p) for p in (Path(__file__).resolve(), extra, lease_tool)}
    for p in startup:
        key = p.relative_to(root).as_posix()
        if key in pins:
            need(startup[p] == pins[key], '開始時helper pin不一致')

    def unchanged():
        need(sha(seal_path) == digest, '元sealが変更されました')
        for key, value in pins.items():
            need(sha(inside(root, key, True)) == value, '待機中にseal pinが変更されました')
        need(all(sha(p) == value for p, value in startup.items()), '開始時helperが変更されました')
        lease_module.load_package(root, package_path, lease['manifest_sha256'])
        validate_atomic(root, m, pinned, lease_module)

    return c, engine, fresh, supp, capture, extra, ntfs, startup, unchanged


def recover(root, seal_path, digest, out, ctx, engine_returncode, engine_output):
    c, engine, fresh, supp, capture, extra, ntfs, startup, unchanged = ctx
    # engineの既result/sealは更新しない。wrapperの証拠は独立subdirだけへ保存する。
    out.mkdir(parents=True, exist_ok=True)
    logdir = inside(root, out / 'unattended-recovery')
    logdir.mkdir(mode=0o700)
    (logdir / 'engine.log').write_bytes(engine_output)
    state = dict(schema='UNATTENDED_SIGNED_RENDER_RECOVERY_V1', status='engine-returned',
                 engine_returncode=engine_returncode, seal_sha256=digest,
                 boot_written=False, gpt_written=False, hardware_success=False,
                 mutation_outcome='unknown', engine_reexecuted=False,
                 startup_pins=[dict(path=p.relative_to(root).as_posix(), sha256=h)
                               for p, h in startup.items()])

    def save():
        (logdir / 'result.json').write_text(json.dumps(state, ensure_ascii=False, indent=2) + '\n')

    save()
    try:
        result_path = inside(root, out / 'result.json', True)
        old_digest = sha(result_path)
        old = read(result_path)
        state['engine_result_sha256'] = old_digest
        if engine_returncode == 0:
            evidence = verify_collected(root, c, digest, old)
            unchanged()
            need(sha(result_path) == old_digest, '旧engine resultが変更されました')
            state.update(status='engine-collection-verified-main-review-required', **evidence)
            save()
            return 0
        need(failure_eligible(old, c, digest), '失敗状態が限定回収条件に一致しません')
        need(all(not p.exists() for p in (fresh, supp)), '既存captureは再利用できません')
        unchanged()
        state.update(status='waiting-for-lease-recovery', prior_status=old['prior_status'],
                     wait_seconds=WAIT_SECONDS)
        save()
        deadline = time.monotonic() + WAIT_SECONDS

        def command(args, name=None, timeout=5, check=False, bounded=True):
            if bounded:
                remaining = deadline - time.monotonic()
                need(remaining > 0, '1900秒のRecovery待機上限に到達しました')
                timeout = min(timeout, remaining)
            try:
                p = subprocess.run(args, cwd=root, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, timeout=timeout)
            except subprocess.TimeoutExpired as exc:
                if name:
                    (logdir / name).write_bytes(exc.output or b'')
                raise
            if name:
                (logdir / name).write_bytes(p.stdout)
            if check:
                p.check_returncode()
            return p

        def adb(*args, **kwargs):
            return command(['adb', '-s', c['serial'], *args], **kwargs)

        def probe(*args):
            try:
                p = adb(*args)
                return p.stdout.strip() if p.returncode == 0 else b''
            except subprocess.TimeoutExpired:
                return b''

        def pause():
            time.sleep(min(5, max(0, deadline - time.monotonic())))

        transition_sent = False
        root_requested = False
        ready = False
        while time.monotonic() < deadline:
            if probe('get-state') == b'recovery':
                # serial確認後にroot要求。機種/領域は既backendのidentityで検証する。
                serial = probe('get-serialno')
                if not serial:
                    pause()
                    continue
                need(serial.decode() == c['serial'], 'Recoveryのserialが不一致です')
                if probe('shell', 'id', '-u') == b'0':
                    ready = True
                    break
                if not root_requested:
                    root_requested = True
                    state['root_requested'] = True
                    save()
                    try:
                        adb('root', name='adb-root.log', timeout=10)
                    except subprocess.TimeoutExpired:
                        pass
            elif not transition_sent:
                try:
                    p = command(['fastboot', '-s', c['serial'], 'getvar', 'product'])
                except subprocess.TimeoutExpired:
                    p = None
                if p is not None and p.returncode == 0:
                    need(re.search(rb'product:\s*A04BR3(?:\s|$)', p.stdout),
                         'fastboot productが不一致です')
                    (logdir / 'fastboot-product.log').write_bytes(p.stdout)
                    transition_sent = True
                    state['recovery_transition_requested'] = True
                    save()
                    try:
                        command(['fastboot', '-s', c['serial'], 'oem', 'reboot-recovery'],
                                name='to-recovery.log', timeout=20, check=True)
                    except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as exc:
                        state['recovery_transition_ack'] = type(exc).__name__
                        save()  # ack不明でも再送しない。Recovery出現だけを待つ。
            pause()
        need(ready, '1900秒以内に同機種Recovery/rootを確認できませんでした')
        unchanged()
        need(sha(result_path) == old_digest, '旧engine resultが変更されました')
        need(all(not p.exists() for p in (fresh, supp)), 'capture出力が既に存在します')
        backend = load('unattended_capture_backend', capture)
        recovery = backend.Recovery(c['serial'])
        identity = recovery.identity()
        recovery.require_unmounted(identity)
        state.update(status='capturing-read-only', recovery_boot_id=identity['recovery_boot_id'])
        save()
        # /tmp helper配置だけ。Windows/boot/ESP/GPTへは書き込まない。
        adb('push', str(ntfs), '/tmp/rt-ntfs3g', name='push-ntfs.log', timeout=120,
            check=True, bounded=False)
        adb('shell', 'chmod', '755', '/tmp/rt-ntfs3g', check=True, bounded=False)
        args = [sys.executable, '-B', str(capture), 'capture', '--serial', c['serial'],
                '--out', str(fresh)]
        for name in engine.TARGET_SYS:
            args += ['--target-sys', name + '.sys']
        args += ['--allow-missing-sys', '--engine-probe-state', '--sd-component-state',
                 '--allow-missing-engine-service-log', '--mount-driver', 'ntfs-3g',
                 '--ntfs3g-remote', '/tmp/rt-ntfs3g']
        command(args, 'capture.log', timeout=1800, check=True, bounded=False)
        mf = inside(root, fresh / 'manifest.json', True)
        m = read(mf)
        need(completed_capture(m) and all(m.get(k) == v for k, v in identity.items()),
             '同Recoveryのprimary ROcapture完了を確認できません')
        primary_digest = sha(mf)
        unchanged()
        command([sys.executable, '-B', str(extra), '--manifest', str(mf),
                 '--manifest-sha256', primary_digest, '--out', str(supp),
                 '--gpu-runtime-assets', '--include-system-dxgi'],
                'gpu-assets.log', timeout=1800, check=True, bounded=False)
        sm = read(inside(root, supp / 'manifest.json', True))
        need(completed_capture(sm) and sm['source_manifest_sha256'] == primary_digest
             and sm['recovery_boot_id'] == identity['recovery_boot_id']
             and sha(mf) == primary_digest and recovery.identity() == identity,
             '同bootのGPU補足/原capture不変を確認できません')
        recovery.require_unmounted(identity)
        need(sha(result_path) == old_digest and sha(seal_path) == digest,
             '旧result/sealが変更されました')
        state.update(status='read-only-evidence-recovered-main-review-required',
                     capture_manifest=c['capture_output'] + '/manifest.json',
                     capture_manifest_sha256=primary_digest,
                     gpu_assets_manifest_sha256=sha(supp / 'manifest.json'))
        save()
        return 0  # 回収の成功だけ。engine失敗/変更結果unknownはstateに保持する。
    except Exception as exc:
        state.update(status='stopped-preserve-evidence', error=str(exc))
        save()
        raise


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--signed-render-capture', type=int, required=True)
    ap.add_argument('--workspace', type=Path, required=True)
    ap.add_argument('--seal', type=Path, required=True)
    ap.add_argument('--seal-sha256', required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--validate-only', action='store_true')
    a = ap.parse_args()
    root = a.workspace.resolve(strict=True)
    need(root == HERE.parents[3] and root.name == 'TAB-A04-BR3', '正本workspaceだけを使用できます')
    seal_path = inside(root, a.seal, True)
    out = inside(root, a.output)
    # 同sealのwrapper同時実行を拒否。seal内容や既engineのlatchは変更しない。
    with seal_path.open('rb') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        ctx = validate(root, seal_path, a.seal_sha256, a.signed_render_capture, out)
        if a.validate_only:
            print('UNATTENDED_SIGNED_RENDER_VALIDATED_NO_DEVICE_OPERATION')
            return 0
        ctx[-1]()
        args = [sys.executable, '-B', str(HERE / 'run-gpu368-fresh200.py'),
                '--signed-render-capture', str(a.signed_render_capture),
                '--workspace', str(root), '--seal', str(seal_path),
                '--seal-sha256', a.seal_sha256, '--output', str(out), '--collection-reboot']
        p = subprocess.run(args, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return recover(root, seal_path, a.seal_sha256, out, ctx, p.returncode, p.stdout)


if __name__ == '__main__':
    sys.exit(main())
