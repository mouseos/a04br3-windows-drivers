"""Validate private native-prepared inputs; never load or modify a hive."""
import hashlib
import json
import re
import struct
from pathlib import Path


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def private_path(path, root):
    path = path.resolve()
    root = root.resolve(strict=True)
    if not path.is_relative_to(root) or path == root:
        raise ValueError('private入力・出力は指定作業ルート内が必要です')
    source_checkout = Path(__file__).resolve().parents[2]
    if path.is_relative_to(source_checkout):
        raise ValueError('private入力・出力をdrivers source checkoutへ置けません')
    return path


def clean_header(path):
    with path.open('rb') as stream:
        header = stream.read(4096)
    if len(header) != 4096 or header[:4] != b'regf':
        raise ValueError('clean SYSTEM base blockが必要です')
    seq = struct.unpack_from('<II', header, 4)
    major, minor, kind, form = struct.unpack_from('<IIII', header, 20)
    extent = struct.unpack_from('<I', header, 40)[0]
    checksum = 0
    for value in struct.unpack('<127I', header[:508]):
        checksum ^= value
    checksum = 0xfffffffe if checksum == 0xffffffff else 1 if checksum == 0 else checksum
    if ((major, minor, kind, form) != (1, 5, 0, 1) or seq[0] != seq[1]
            or checksum != struct.unpack_from('<I', header, 508)[0]
            or extent % 4096 or path.stat().st_size < 4096 + extent):
        raise ValueError('SYSTEM format 1.5/clean sequence/checksum/extent不一致')
    return {'major': major, 'minor': minor, 'clean': True, 'checksum_valid': True}


def prepared_system_check(system, manifest, expected_sha256, root, backup, selected_service="MT8167HardwareProbe"):
    names = {'MT8167HardwareProbe': 'MediaTek MT8167 Hardware Probe', 'MT8167EngineProbe': 'MediaTek MT8167 Engine Probe', 'MT8167RenderProbe': 'PowerVR GE8300 Render Probe', 'MT8167RenderStage': 'PowerVR GE8300 Driver Update', 'GE8300RenderSetup': 'PowerVR GE8300 Driver Update'}
    if selected_service not in names:
        raise ValueError('未対応の固定serviceです')
    system = private_path(system, root).resolve(strict=True)
    manifest = private_path(manifest, root).resolve(strict=True)
    if not re.fullmatch('[0-9a-fA-F]{64}', expected_sha256 or ''):
        raise ValueError('prepared SYSTEMの完全SHA256 pinが必要です')
    report = json.loads(manifest.read_text(encoding='utf-8-sig'))
    if (report.get('status') != 'verified-clean-system-services-only'
            or report.get('whole_system_audit') is not False or not report.get('audit_scope')
            or report.get('services') != [selected_service]):
        raise ValueError('HardwareProbeだけをnative準備した成功manifestが必要です')
    candidate = report['candidate']
    native_path = candidate['path'].replace('\\', '/')
    if native_path not in ('candidate/SYSTEM', 'SYSTEM'):
        raise ValueError('native candidateの相対path契約不一致')
    declared = (manifest.parent / native_path).resolve()
    # Host recovery may flatten candidate/SYSTEM to SYSTEM beside result.json.
    allowed = {declared, (manifest.parent / 'SYSTEM').resolve()}
    if (system not in allowed or not system.is_relative_to(manifest.parent)
            or sha(system) != expected_sha256.lower()
            or candidate['sha256'].lower() != expected_sha256.lower()):
        raise ValueError('candidate path/SHA256 pin不一致')
    header = clean_header(system)
    # Native recipe compares these exact seven values on patch and clean reload.
    expected = {
        'Type': ('DWord', 16), 'Start': ('DWord', 2), 'ErrorControl': ('DWord', 1),
        'ImagePath': ('ExpandString', '"C:\\Windows\\System32\\' + selected_service + '.exe"'),
        'ObjectName': ('String', 'LocalSystem'),
        'DisplayName': ('String', names[selected_service]),
        'DependOnService': ('MultiString', ['DeviceInstall', 'RpcSs']),
    }
    baseline = None
    for phase in ('patch', 'verify'):
        worker = report[phase]
        state = worker['state']
        if (worker.get('finished') is not True or worker.get('exit_code') != 0
                or state.get('status') != 'success' or state.get('load_rc') != 0
                or state.get('unload_rc') != 0 or state.get('errors')
                or state.get('whole_system_audit') is not False or not state.get('audit_scope')):
            raise ValueError('native patch/clean reload/unload成功の証拠が必要です')
        if report.get('engine_rearm_plan_sha256') and state.get('engine_rearm_plan_sha256') != report['engine_rearm_plan_sha256']:
            raise ValueError('native patch/verifyの再arm plan binding不一致')
        services = state['services']
        if len(services) != 1 or services[0]['name'] != selected_service:
            raise ValueError('native workerの対象service不一致')
        values = services[0]['values']
        if set(values) != set(expected):
            raise ValueError('HardwareProbeの検証対象は7値だけです')
        for name, (kind, value) in expected.items():
            if values[name]['kind'] != kind or values[name]['value'] != value:
                raise ValueError('HardwareProbe値または型不一致: ' + name)
        before, after = state['related_fingerprints_before'], state['related_fingerprints_after']
        if set(before) != {'select', 'device_install', 'rpc_ss'} or before != after:
            raise ValueError('関連既存設定の限定fingerprint不一致')
        if baseline is not None and before != baseline:
            raise ValueError('native patchとclean reloadの関連設定不一致')
        baseline = before
    source = {item['source_path'].replace('\\', '/'): item for item in backup['files']}
    for name in ('SYSTEM', 'SYSTEM.LOG1', 'SYSTEM.LOG2'):
        recorded = report['source_files'][name]
        original = source['Windows/System32/config/' + name]
        if (recorded['sha256'].lower() != original['sha256'].lower()
                or recorded['bytes'] != original['bytes']):
            raise ValueError('native入力とRecovery backupの由来不一致: ' + name)
    return {'sha256': expected_sha256.lower(), 'manifest_sha256': sha(manifest),
            'root_relative_path': str(system.relative_to(root.resolve())), 'header': header,
            'native_candidate_path': native_path, 'relocated_after_native_preparation': system != declared,
            'status': report['status'], 'whole_system_audit': False,
            'audit_scope': report['audit_scope'], 'selected_service': selected_service, 'target_value_names': list(expected),
            'engine_rearm_plan_sha256': report.get('engine_rearm_plan_sha256'),
            'disabled_service_fingerprint': report['patch']['state'].get('disabled_service_fingerprint')}


def probe_check(executable, manifest, certificate, root, audio_mode="Tone"):
    paths = [p.resolve(strict=True) for p in (executable, manifest, certificate)]
    if any(not p.is_relative_to(root.resolve()) or not p.is_file() for p in paths):
        raise ValueError('probe EXE/manifest/certは作業ルート内のfileが必要です')
    executable, manifest, certificate = paths
    engine = audio_mode == 'EnginePcm'
    service = 'MT8167EngineProbe' if engine else 'MT8167HardwareProbe'
    audio = 'mt8167-audio-engine-probe.exe' if engine else 'mt8167-audio-first-playback.exe'
    report = json.loads(manifest.read_text(encoding='utf-8-sig'))
    if engine:
        brightness=report.get('build_defines',{}).get('A04_ENGINE_BRIGHTNESS_CLIENT')=='1'
        expected_clients=['mt6630-wlan-probe.exe',audio]+(['mt8167-brightness-native.exe'] if brightness else [])
        if brightness and not report.get('deferred_update'):raise ValueError('brightness clientは明示延期batch専用です')
        if (report.get('audio_mode') != 'engine_pcm_retained' or report.get('engine_pcm_compatible') is not True
                or report.get('fixed_clients') != expected_clients
                or report.get('build_defines', {}).get('A04_HARDWARE_PROBE_ENGINE_PCM') != '1'):
            raise ValueError('Engine専用service binary/manifestが必要です')
        if (report.get('service_destination') != 'System32\\MT8167EngineProbe.exe'
                or report.get('client_directory') != 'System32\\MT8167EngineProbe\\clients'
                or report.get('log_directory') != 'System32\\MT8167EngineProbe\\logs'
                or report.get('marker_directory') != 'System32\\MT8167EngineProbe'):
            raise ValueError('Engine専用service/client/log/marker path不一致')
        pnp = report.get('pnp_rebind', {})
        deferred = report.get('deferred_update')
        if deferred is not None:
            from engine_deferred_policy import TARGETS
            has_gpu='gpu' in deferred.get('packages',{})
            has_sd='sd' in deferred.get('packages',{})
            expected_ids=[x[0] for x in TARGETS.values()]+(['ACPI\\MOU0006'] if has_gpu else [])+(['ACPI\\MOU0013'] if has_sd else [])
            if (deferred.get('mode') != 'engine_fixed_deferred_v1' or deferred.get('schema') != 1
                    or set(deferred.get('packages', {})) != (set(TARGETS)|({'gpu'} if has_gpu else set())|({'sd'} if has_sd else set()))
                    or pnp.get('api') != 'SetupDiCallClassInstaller/DIF_INSTALLDEVICE'
                    or pnp.get('fixed_hardware_ids') != expected_ids
                    or pnp.get('parent_policy') != 'DI_DONOTCALLCONFIGMG-deferred-reboot-only'
                    or pnp.get('live_parent_replacement') is not False
                    or pnp.get('parent_updated') is not True
                    or pnp.get('directory') != 'System32\\MT8167EngineProbe\\drivers'
                    or pnp.get('phases') != ['install-once','planned-reboot-once','verify-and-diagnose-once']
                    or pnp.get('problem_status') != 'CM_Get_DevNode_PropertyW/DEVPKEY_Device_ProblemStatus/DEVPROP_TYPE_NTSTATUS'
                    or pnp.get('maximum_calls_per_id') != 1
                    or pnp.get('catalog_signature_required') is not True
                    or pnp.get('reboot_required_stops_clients') is not True
                    or report.get('build_defines',{}).get('A04_ENGINE_DEFERRED_UPDATE') != '1'):
                raise ValueError('固定Engine延期更新phase契約不一致')
            if set(deferred.get('clients',{}))!=set(expected_clients):raise ValueError('固定batch client数/名前不一致')
            policy_path=manifest.parent/'deferred-policy-manifest.json'
            if (sha(policy_path) != report.get('deferred_policy_manifest_sha256')
                    or json.loads(policy_path.read_text(encoding='utf-8-sig')) != deferred):
                raise ValueError('署名serviceの延期policy実file pin不一致')
        else:
            if report.get('build_defines',{}).get('A04_ENGINE_DEFERRED_UPDATE') == '1':raise ValueError('延期buildのpolicy欠落')
            if (pnp.get('api') != 'UpdateDriverForPlugAndPlayDevicesW' or pnp.get('flags') != 5
                    or pnp.get('fixed_hardware_ids') not in (['MT8167\\AFE', 'MT8167\\USB20', 'A04BR3\\MT6630SDIO'], ['ACPI\\BEN0001', 'MT8167\\AFE', 'MT8167\\USB20', 'A04BR3\\MT6630SDIO'])
                    or pnp.get('directory') != 'System32\\MT8167EngineProbe\\drivers'
                    or pnp.get('maximum_calls_per_id') != 1
                    or pnp.get('parent_updated') is not (len(pnp.get('fixed_hardware_ids', [])) == 4)
                    or pnp.get('reboot_required_stops_clients') is not True
                    or pnp.get('catalog_signature_required') is not True):
                raise ValueError('Engine固定PnP再bind契約のあるserviceが必要です')
        binary = executable.read_bytes()
        if audio.encode('utf-16le') not in binary or 'mt8167-audio-first-playback.exe'.encode('utf-16le') in binary:
            raise ValueError('旧tone binaryまたは別clientをEngineProbeへ混用できません')
    if engine and brightness and 'mt8167-brightness-native.exe'.encode('utf-16le') not in executable.read_bytes():raise ValueError('固定第3clientがbinaryにありません')
    cert = certificate.read_bytes()
    thumbprint = hashlib.sha1(cert).hexdigest().upper()
    files = {item['name']: item['sha256'].lower() for item in report['files']}
    if (report.get('service') != service or report.get('signing_requested') is not True
            or report.get('source_dirty') is not False or not cert.startswith(b'\x30')
            or report.get('signing_thumbprint', '').upper() != thumbprint
            or files.get(service + '.exe') != sha(executable)
            or report.get('scm_start') != 2 or report.get('scm_arguments') != ''
            or report.get('audio_interface') != 11 or (not engine and report.get('engine_pcm_compatible') is True)):
        raise ValueError('probe EXE hash/署名manifest/cert thumbprint契約不一致')
    return {'source_commit': report['source_commit'], 'service': service,
            'audio_mode': 'engine_pcm_retained' if engine else 'private_fixed_tone_diagnostic', 'exe_sha256': sha(executable),
            'manifest_sha256': sha(manifest), 'certificate_sha256': sha(certificate),
            'certificate_thumbprint': thumbprint, 'authenticode_verified_here': False,
            'pnp_rebind': report.get('pnp_rebind') if engine else None,
            'deferred_update': report.get('deferred_update') if engine else None,
            'deferred_policy_manifest_sha256': report.get('deferred_policy_manifest_sha256') if engine else None,
            'deferred_runtime_source_sha256':report.get('deferred_runtime_source_sha256') if engine else None}


def certificate_trust_commands(certificates):
    commands, seen = [], set()
    for der in certificates.values():
        digest = hashlib.sha1(der).digest()
        thumb = digest.hex().upper()
        if thumb in seen:
            continue
        seen.add(thumb)
        blob = struct.pack('<III', 3, 1, 20) + digest + struct.pack('<III', 32, 1, len(der)) + der
        for store in ('Root', 'TrustedPublisher'):
            key = 'HKLM\\Mt8167IoOffline\\Microsoft\\SystemCertificates\\' + store + '\\Certificates\\' + thumb
            commands.extend(['reg.exe add "' + key + '" /v Blob /t REG_BINARY /d ' + blob.hex() + ' /f >> "%A04_REPORT%" 2>&1',
                             'if errorlevel 1 goto trustfail'])
    return commands
