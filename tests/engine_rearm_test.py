"""One compact private rearm/installed-SYS regression; synthetic files only."""
import argparse
import copy
import importlib.util
import json
import sys
import tempfile
from pathlib import Path

repo = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(repo / 'tools/winpe'))
from engine_rearm import make_plan, check_plan, archive_commands, DISABLED_FINGERPRINT, KINDS, SYS_NAMES, STATE_FILES
from private_hardware_probe_input import sha
spec = importlib.util.spec_from_file_location('installed', repo / 'tools/recovery/verify-engine-installed.py')
installed = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installed)
parser = argparse.ArgumentParser()
parser.add_argument('--scratch', type=Path, required=True)
args = parser.parse_args()
with tempfile.TemporaryDirectory(dir=args.scratch, prefix='engine-rearm-contract-') as temp:
    root = Path(temp)
    backup = {'status': 'captured-unmounted', 'recovery_boot_id': '22222222-2222-2222-2222-222222222222', 'files': []}
    for name in ('SYSTEM', 'SYSTEM.LOG1', 'SYSTEM.LOG2'):
        backup['files'].append({'source_path': 'Windows/System32/config/' + name, 'sha256': '0' * 64, 'bytes': 4096})
    import hashlib
    old_service = hashlib.sha256(b'synthetic old service').hexdigest()
    for name, data in zip(STATE_FILES, (b'MT8167EngineProbe attempted\n', b'synthetic disk log', b'synthetic old service')):
        backup['files'].append({'source_path': name, 'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)})
    components = {}
    for kind in KINDS:
        sys_name = SYS_NAMES[kind]
        data = ('synthetic next ' + kind).encode()
        digest = hashlib.sha256(data).hexdigest()
        components[kind] = {'driver_ver': '11/06/2026,2026.11.6.1001',
                            'files': {kind + '/' + sys_name: digest, kind + '/fixed.inf': '1' * 64}}
        local = root / 'files' / sys_name
        local.parent.mkdir(exist_ok=True)
        local.write_bytes(b'synthetic stale parent' if kind == 'parent' else data)
        backup['files'].append({'source_path': 'Windows/System32/Drivers/' + sys_name,
                               'backup_path': 'files/' + sys_name, 'sha256': sha(local), 'bytes': local.stat().st_size})
    backup_path = root / 'backup.json'
    backup_path.write_text(json.dumps(backup))
    previous = {'audio_mode': 'engine_pcm_retained', 'selected_service': 'MT8167EngineProbe',
                'private_do_not_distribute': True, 'recovery_boot_id': '11111111-1111-1111-1111-111111111111',
                'host_backup_manifest_sha256': 'f' * 64, 'components': copy.deepcopy(components),
                'probe': {'exe_sha256': old_service}}
    old_path = root / 'old-media.json'
    old_path.write_text(json.dumps(previous))
    try:
        make_plan(old_path, backup_path, backup, components, root)
    except ValueError:
        pass
    else:
        raise AssertionError('same-version stale-parent rearm accepted')
    components['parent']['driver_ver'] = '11/06/2026,2026.11.7.1005'
    components['parent']['files']['parent/fixed.inf'] = '2' * 64
    plan = make_plan(old_path, backup_path, backup, components, root)
    plan_path = root / 'plan.json'
    plan_path.write_text(json.dumps(plan))
    native = {'engine_rearm_plan_sha256': sha(plan_path), 'disabled_service_fingerprint': DISABLED_FINGERPRINT}
    assert check_plan(plan_path, sha(plan_path), backup_path, backup, components, native, root) == plan
    wrong_native = dict(native, disabled_service_fingerprint='0' * 64)
    try:
        check_plan(plan_path, sha(plan_path), backup_path, backup, components, wrong_native, root)
    except ValueError:
        pass
    else:
        raise AssertionError('unknown service fingerprint accepted')
    commands = archive_commands(sha(plan_path))
    assert 'move C:\\Windows\\System32\\MT8167EngineProbe' in commands
    assert 'del ' not in commands.lower() and 'rmdir ' not in commands.lower()
    # A newer backup can still contain an old SYS: capture success is not installation success.
    media = dict(previous, expected_installed_sys=plan['expected_installed_sys'])
    media_path = root / 'next-media.json'
    media_path.write_text(json.dumps(media))
    result = installed.verify(media_path, sha(media_path), backup_path, sha(backup_path), root)
    assert result['status'] == 'installed-sys-mismatch'
    parent = root / 'files' / SYS_NAMES['parent']
    parent.write_bytes(b'synthetic next parent')
    for entry in backup['files']:
        if entry['source_path'].endswith(SYS_NAMES['parent']):
            entry.update(sha256=sha(parent), bytes=parent.stat().st_size)
    backup_path.write_text(json.dumps(backup))
    result = installed.verify(media_path, sha(media_path), backup_path, sha(backup_path), root)
    assert result['status'] == 'all-installed-sys-hashes-match'
    assert result['live_loaded_image_verified'] is False and result['functional_io_verified'] is False
print('Explicit rearm/known fingerprint/version gate/archive retention and stale-SYS detection passed; synthetic only.')
