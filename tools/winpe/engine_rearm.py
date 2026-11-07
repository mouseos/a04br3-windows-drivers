"""Explicit private Engine rearm contract. No device access and no hive editing."""
import datetime
import hashlib
import json
import re
from pathlib import Path
from private_hardware_probe_input import private_path, sha

DISABLED_FINGERPRINT = 'dbbe363d2401dfc022606aba010c0e4888c5feab67c748ea5030b0eb3ba484c1'
KINDS = ('parent', 'audio', 'usb', 'wifi')
MARKER = 'Windows/System32/MT8167EngineProbe/attempted.bin'
STATE_FILES = (MARKER, 'Windows/System32/MT8167EngineProbe/logs/service.log',
               'Windows/System32/MT8167EngineProbe.exe')
BRIGHTNESS_LOG='MT8167EngineProbe/brightness-native.log'
SYS_NAMES = {'parent': 'mt8167-shared.sys', 'audio': 'mt8167-audio.sys',
             'usb': 'mt8167-usb20.sys', 'wifi': 'mt6630-runtime.sys',
             'gpu':'mt8167-ge8300.sys', 'display': 'mt8167-display.sys', 'toprgu': 'mt8167-toprgu.sys'}


def version(value, gpu=False):
    date, numbers = value.split(',')
    if (not gpu and date != '11/06/2026') or not re.fullmatch(r'\d+\.\d+\.\d+\.\d+', numbers):
        raise ValueError('再arm INFは固定dateと4要素versionが必要です')
    if gpu:datetime.datetime.strptime(date,'%m/%d/%Y')
    return tuple(map(int, numbers.split('.')))


def package_state(components):
    return {kind: {'driver_ver': components[kind]['driver_ver'],
                   'files': components[kind]['files']} for kind in (*KINDS, 'display', 'toprgu', 'gpu') if kind in components}


def require_progress(previous, following, backup):
    changed = []
    actual = {entry['source_path'].replace('\\', '/').casefold(): entry['sha256'].lower()
              for entry in backup['files']}
    for kind in following:
        if kind not in previous:
            expected=next(digest.lower() for name,digest in following[kind]['files'].items() if name.endswith('.sys'))
            if actual.get('windows/system32/drivers/'+SYS_NAMES[kind])==expected:continue
            raise ValueError('旧版のcomponent/version metadataが必要です: ' + kind)
        before, after = previous[kind], following[kind]
        if before['files'] != after['files']:
            if version(after['driver_ver'],kind=='gpu') <= version(before['driver_ver'],kind=='gpu'):
                raise ValueError('修正packageには旧版より高いDriverVerが必要です: ' + kind)
            changed.append(kind)
        path = 'windows/system32/drivers/' + SYS_NAMES[kind]
        expected = next(digest.lower() for name, digest in after['files'].items() if name.endswith('.sys'))
        if actual.get(path) != expected and version(after['driver_ver'],kind=='gpu') <= version(before['driver_ver'],kind=='gpu'):
            raise ValueError('実SYS未適用/不存在の対象はINF昇版が必要です: ' + kind)
    if not changed:
        raise ValueError('修正package hashが無い同一構成の再armは拒否します')
    return changed


def missing_log_binding(backup, backup_path, exe_hash, allowed):
    log = STATE_FILES[1]
    entries = {e['source_path'].replace('\\', '/'): e for e in backup['files']}
    missing = [e for e in backup.get('missing_files', []) if e.get('source_path', '').replace('\\', '/') == log]
    if log in entries:
        if missing or not entries[log]['bytes']:
            raise ValueError('旧service.logのpresent/absent状態が矛盾または空です')
        return None
    if (not allowed or backup.get('engine_missing_service_log_opt_in') is not True
            or missing != [{'source_path': log, 'reason': 'absent'}]):
        raise ValueError('旧service.log欠落は明示opt-in captureとabsent記録が必要です')
    snapshot = {'backup_manifest_sha256': sha(backup_path), 'service_log': missing[0],
                'previous_service_exe_sha256': exe_hash,
                'marker': {k: entries[MARKER][k] for k in ('sha256', 'bytes')}}
    return {'snapshot': snapshot, 'snapshot_sha256': hashlib.sha256(
        json.dumps(snapshot, sort_keys=True, separators=(',', ':')).encode()).hexdigest()}


def make_plan(previous_path, backup_path, backup, components, root, allow_missing_service_log=False):
    previous_path = private_path(previous_path, root).resolve(strict=True)
    previous = json.loads(previous_path.read_text())
    if (previous.get('audio_mode') != 'engine_pcm_retained'
            or previous.get('selected_service') != 'MT8167EngineProbe'
            or previous.get('private_do_not_distribute') is not True):
        raise ValueError('旧Engine private媒体manifestが必要です')
    if (previous.get('recovery_boot_id') == backup['recovery_boot_id']
            or previous.get('host_backup_manifest_sha256') == sha(backup_path)):
        raise ValueError('使用済み媒体と同じbackupは再armに使えません')
    before, after = package_state(previous['components']), package_state(components)
    if 'gpu' in after and 'gpu' not in before:
        before['gpu']={k:components['gpu']['baseline'][k] for k in ('driver_ver','files')}
    if 'gpu' in after:
        actual={x['source_path'].replace('\\','/').lower():x['sha256'].lower() for x in backup['files']}
        if actual.get('windows/system32/drivers/mt8167-ge8300.sys')!=next(v for k,v in before['gpu']['files'].items() if k.endswith('.sys')):raise ValueError('GPU baselineとfreshbackup実SYS hash不一致')
    changed = require_progress(before, after, backup)
    entries = {entry['source_path'].replace('\\', '/'): entry for entry in backup['files']}
    if not {MARKER, STATE_FILES[2]}.issubset(entries):
        raise ValueError('fresh backupへ--engine-probe-stateでmarker/service.log/EXEを回収してください')
    old_service_hash=previous.get('probe',{}).get('exe_sha256')
    if not old_service_hash or entries[STATE_FILES[2]]['sha256'].lower()!=old_service_hash.lower():
        raise ValueError('旧service EXEが旧媒体の固定署名hashと一致しません')
    marker = entries[MARKER]
    if marker['sha256'].lower() != hashlib.sha256(b'MT8167EngineProbe attempted\n').hexdigest():
        raise ValueError('既知Engine attempted.binのfingerprintと一致しません')
    missing_log = missing_log_binding(backup, backup_path, old_service_hash, allow_missing_service_log)
    old_state_names=[n for n in STATE_FILES if n in entries]
    if BRIGHTNESS_LOG in entries:old_state_names.append(BRIGHTNESS_LOG)
    elif 'mt8167-brightness-native.exe' in previous.get('fixed_clients',[]):
        if BRIGHTNESS_LOG not in {x['source_path'].replace('\\','/') for x in backup.get('missing_files',[])}:
            raise ValueError('旧固定brightness logの回収または明示absent記録が必要です')
    plan={'schema': 1, 'service': 'MT8167EngineProbe', 'maximum_rearms': 1,
            'disabled_service_fingerprint': DISABLED_FINGERPRINT,
            'backup_manifest_sha256': sha(backup_path), 'recovery_boot_id': backup['recovery_boot_id'],
            'previous_media_manifest_sha256': sha(previous_path),
            'previous_packages': before, 'next_packages': after, 'changed_components': changed,
            'source_files': {name: {'sha256': entries['Windows/System32/config/' + name]['sha256'],
                                    'bytes': entries['Windows/System32/config/' + name]['bytes']}
                             for name in ('SYSTEM', 'SYSTEM.LOG1', 'SYSTEM.LOG2')},
            'old_state_files': {name: {'sha256': entries[name]['sha256'], 'bytes': entries[name]['bytes']}
                                for name in old_state_names},
            'expected_installed_sys': {SYS_NAMES[kind]: next(digest.lower() for name, digest in after[kind]['files'].items()
                                                            if name.endswith('.sys')) for kind in (*KINDS, 'display', 'toprgu', 'gpu') if kind in components}}
    if missing_log:
        plan['missing_service_log'] = missing_log
    plan['previous_service_exe_sha256'] = old_service_hash.lower()
    if 'gpu' in components:
        gpu=components['gpu'];plan['gpu_transition']={k:gpu[k] for k in ('source_commit','policy_manifest_sha256','transition_id')}
        plan['gpu_transition'].update(candidate_source_commit=gpu['source_commit'],candidate_sys_sha256=gpu['files']['gpu/mt8167-ge8300.sys'],candidate_cat_sha256=gpu['files']['gpu/mt8167-ge8300.cat'])
    return plan


def check_plan(path, expected_sha, backup_path, backup, components, native, root):
    path = private_path(path, root).resolve(strict=True)
    if not re.fullmatch('[0-9a-fA-F]{64}', expected_sha or '') or sha(path) != expected_sha.lower():
        raise ValueError('再arm plan SHA256 pin不一致')
    plan = json.loads(path.read_text())
    if (plan.get('schema') != 1 or plan.get('service') != 'MT8167EngineProbe'
            or plan.get('maximum_rearms') != 1 or plan.get('disabled_service_fingerprint') != DISABLED_FINGERPRINT
            or plan.get('backup_manifest_sha256') != sha(backup_path)
            or plan.get('recovery_boot_id') != backup['recovery_boot_id']
            or plan.get('next_packages') != package_state(components)
            or native.get('engine_rearm_plan_sha256') != expected_sha.lower()
            or native.get('disabled_service_fingerprint') != DISABLED_FINGERPRINT):
        raise ValueError('再arm plan/修正package/freshbackup/nativeのbinding不一致')
    if 'gpu' in components:
        gpu=components['gpu'];binding={k:gpu[k] for k in ('source_commit','policy_manifest_sha256','transition_id')}
        binding.update(candidate_source_commit=gpu['source_commit'],candidate_sys_sha256=gpu['files']['gpu/mt8167-ge8300.sys'],candidate_cat_sha256=gpu['files']['gpu/mt8167-ge8300.cat'])
        if plan.get('gpu_transition')!=binding:raise ValueError('再arm GPU transition/policy/hash不一致')
    if 'gpu' in components and plan['previous_packages'].get('gpu')!={k:components['gpu']['baseline'][k] for k in ('driver_ver','files')}:raise ValueError('再arm GPU baseline metadata不一致')
    require_progress(plan['previous_packages'], plan['next_packages'], backup)
    entries = {x['source_path'].replace('\\', '/'): x for x in backup['files']}
    if plan['old_state_files'].get(MARKER,{}).get('sha256','').lower() != hashlib.sha256(b'MT8167EngineProbe attempted\n').hexdigest():
        raise ValueError('旧markerの既知fingerprint不一致')
    for name, old in plan['old_state_files'].items():
        if name not in (*STATE_FILES,BRIGHTNESS_LOG) or name not in entries or entries[name]['sha256'].lower() != old['sha256'].lower() or entries[name]['bytes'] != old['bytes']:
            raise ValueError('旧Engine state backup fingerprint不一致')
    if not {MARKER, STATE_FILES[2]}.issubset(entries):
        raise ValueError('既知marker/旧EXEの実snapshot欠落')
    missing_log=missing_log_binding(backup, backup_path, plan.get('previous_service_exe_sha256'), 'missing_service_log' in plan)
    if missing_log != plan.get('missing_service_log'):
        raise ValueError('旧log欠落snapshot/hashとfreshbackup不一致')
    if missing_log and entries[STATE_FILES[2]]['sha256'].lower()!=plan.get('previous_service_exe_sha256'):
        raise ValueError('旧service EXE pin不一致')
    expected_state={n for n in STATE_FILES if n in entries}|({BRIGHTNESS_LOG} if BRIGHTNESS_LOG in entries else set())
    if set(plan['old_state_files']) != expected_state:
        raise ValueError('旧Engine state保存対象欠落')
    return plan


def archive_commands(plan_sha, plan=None):
    # A destination collision is permanent refusal, even following partial failure.
    archive = 'C:\\a04backup\\engine-rearm-' + plan_sha
    base = 'C:\\Windows\\System32\\MT8167EngineProbe'
    commands=[
        'if not exist ' + base + '\\attempted.bin goto dismfail',
        ('if exist ' if plan and plan.get('missing_service_log') else 'if not exist ') + base + '\\logs\\service.log goto dismfail',
        'if exist ' + archive + ' goto dismfail',
        'mkdir ' + archive + ' >> "%A04_REPORT%" 2>&1',
        'if errorlevel 1 goto dismfail',
        'move ' + base + ' ' + archive + '\\state >> "%A04_REPORT%" 2>&1',
        'if errorlevel 1 goto dismfail',
        'if exist ' + base + ' goto dismfail',
        'if not exist ' + archive + '\\state\\attempted.bin goto dismfail',
        'copy /b /y C:\\Windows\\System32\\MT8167EngineProbe.exe ' + archive + '\\previous-service.exe >> "%A04_REPORT%" 2>&1',
        'if errorlevel 1 goto dismfail',
        'copy /b /y X:\\a04br3\\engine-rearm-plan.json ' + archive + '\\rearm-plan.json >> "%A04_REPORT%" 2>&1',
        'if errorlevel 1 goto dismfail',
        '>> "%A04_REPORT%" echo ENGINE_REARM_PLAN_SHA256=' + plan_sha,
    ]
    if plan and BRIGHTNESS_LOG in plan.get('old_state_files',{}):
        commands+=['if not exist C:\\MT8167EngineProbe\\brightness-native.log goto dismfail',
            'move C:\\MT8167EngineProbe\\brightness-native.log '+archive+'\\brightness-native.log >> "%A04_REPORT%" 2>&1',
            'if errorlevel 1 goto dismfail','if exist C:\\MT8167EngineProbe\\brightness-native.log goto dismfail',
            'if not exist '+archive+'\\brightness-native.log goto dismfail']
    return '\n'.join(commands)
