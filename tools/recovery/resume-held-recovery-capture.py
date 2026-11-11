#!/usr/bin/env python3
"""移行前に失敗したheld回収を、既にRecoveryにいる場合だけ再開する。"""
import argparse
import hashlib
import importlib.util
import json
import subprocess
import sys
from pathlib import Path


def admit(root, seal, digest):
    if hashlib.sha256(seal.read_bytes()).hexdigest() != digest:
        raise ValueError('seal SHA mismatch before loading controller')
    raw = json.loads(seal.read_text())
    engine = (root / raw['controller_path']).resolve(strict=True)
    if not engine.is_relative_to(root):
        raise ValueError('controller outside workspace')
    actual = hashlib.sha256(engine.read_bytes()).hexdigest()
    if not any(row['path'] == raw['controller_path'] and row['sha256'] == actual
               for row in raw['pinned_files']):
        raise ValueError('unsealed controller')
    sys.path.insert(0, str(engine.parent))
    spec = importlib.util.spec_from_file_location('sealed_capture_engine', engine)
    e = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(e)
    profile = 'peripheral-updates-233'
    c, _ = e.validate_seal(root, seal, digest, profile, str(engine))
    k = e.contract(profile)
    held = e.rooted(root, k['runtime_output'])
    latch = root / ('port/gsi/logs/ios-feasibility/peripherals-run%d-executions' % k['run']) / (k['transaction'] + '.json')
    st = e.verify_collection(held, c, digest, k, latch)
    failed_path = held / 'collection/result.json'
    failed = e.read(failed_path)
    e.require(failed['status'] == 'pipeline-stopped' and
              failed['prior_status'] == st['collection_boundary'] and
              failed.get('collection_only') is True and
              failed.get('held_result_sha256') == e.sha(held / 'result.json') and
              failed.get('seal_sha256') == digest and
              failed.get('transaction') == st['transaction'], 'unrelated failed collection')
    # Only a failure before capture preparation can use this entrypoint.
    e.require(not any(p.name != 'result.json' and p.name != 'terminal-to-recovery.log'
                      for p in (held / 'collection').iterdir()), 'collection already advanced')
    for key in ('capture_output', 'diagnostics_output'):
        e.require(not (root / c[key]).exists(), 'capture output already exists')
    output = held / 'collection-recovery-ready'
    e.require(not output.exists(), 'recovery-ready attempt already exists')
    return e, c, k, st, failed_path, output


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--workspace', required=True, type=Path)
    ap.add_argument('--seal', required=True, type=Path)
    ap.add_argument('--seal-sha256', required=True)
    ap.add_argument('--validate-only', action='store_true')
    a = ap.parse_args()
    root = a.workspace.resolve(strict=True)
    seal = a.seal.resolve(strict=True)
    if not seal.is_relative_to(root):
        raise ValueError('seal outside workspace')
    e, c, k, held, failed_path, out = admit(root, seal, a.seal_sha256)
    if a.validate_only:
        print('HELD_RECOVERY_CAPTURE_VALIDATED_NO_DEVICE_OPERATION')
        return
    # Check transport before claiming the one-shot namespace. Never reset or reboot.
    state = subprocess.run(['adb', '-s', c['serial'], 'get-state'],
                           timeout=12, capture_output=True, check=True)
    e.require(state.stdout.strip() == b'recovery', 'already-running Recovery required')
    out.mkdir(mode=0o700)
    st = dict(held, status=held['collection_boundary'], collection_only=True,
              held_result_sha256=e.sha(out.parent / 'result.json'),
              failed_collection_sha256=e.sha(failed_path), recovery_ready_resume=True)
    def save():
        (out / 'result.json').write_text(json.dumps(st, indent=2) + '\n')
    def command(body, name, args=()):
        with (out / name).open('wb') as log:
            e.run_command_group(['bash', '-c', 'set -euo pipefail\nsource "$1"\n' +
                e.BOUNDED_DEVICE_SHELL + '[ "$rt_target_serial" = "' + c['serial'] + '" ]\n' +
                '[ "$(adb get-state)" = recovery ]\n' +
                '[ "$(adb shell id -u | tr -d "\\r")" = 0 ]\n' + e.MOUNT_GUARD + body,
                'held-capture', str(root / c['target_tools']), *map(str, args)],
                cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=180)
    save()
    try:
        command('adb push "$2" /tmp/rt-ntfs3g\nadb shell chmod 755 /tmp/rt-ntfs3g\n',
                'prepare-capture.log', (root / c['ntfs3g'],))
        args = ['python3', '-B', str(root / c['capture_tool']), 'capture',
                '--serial', c['serial'], '--out', str(root / c['capture_output'])]
        for name in e.TARGET_SYS:
            args += ['--target-sys', name + '.sys']
        args += ['--allow-missing-sys', '--engine-probe-state', '--sd-component-state',
                 '--allow-missing-engine-service-log', '--mount-driver', 'ntfs-3g',
                 '--ntfs3g-remote', '/tmp/rt-ntfs3g']
        with (out / 'capture.log').open('wb') as log:
            e.run_command_group(args, cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=1800)
        cap = e.read(root / c['capture_output'] / 'manifest.json')
        e.require(cap['status'] == 'captured-unmounted' and cap['read_only'] is True and
                  cap['unmounted'] is True and cap['errors'] == [], 'capture not safely unmounted')
        args = ['bash', str(root / c['diagnostics_collector']), str(seal),
                str(out / 'result.json'), str(root / c['diagnostics_output'])]
        if c.get('gpu_runtime_assets', False):
            args += ['--gpu-runtime-assets']
        with (out / 'collect-combined200.log').open('wb') as log:
            e.run_command_group(args, cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=1800)
        index = root / c['diagnostics_output'] / 'capture-index.json'
        e.require(e.read(index)['windows_unmounted'] is True, 'diagnostics not unmounted')
        st.update(status='capture%d-and-diagnostics-ready-for-main-review' % k['capture'],
                  capture_manifest_sha256=e.sha(root / c['capture_output'] / 'manifest.json'),
                  diagnostics_index_sha256=e.sha(index), hardware_success=False)
        save()
    except Exception as exc:
        st.update(prior_status=st['status'], status='pipeline-stopped', error=str(exc), hardware_success=False)
        save()
        raise


if __name__ == '__main__':
    main()
