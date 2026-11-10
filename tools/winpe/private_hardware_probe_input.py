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


def prepared_system_check(system, manifest, expected_sha256, root, backup, selected_service="MT8167HardwareProbe", expected_peripheral_policy=None):
    names = {'MT8167CombinedFollowup200':'MediaTek PowerVR GE8300 verification','MT8167CombinedFollowup196':'MediaTek PowerVR GE8300 verification','MT8167CombinedFollowup194':'MediaTek PowerVR GE8300 verification','MT8167CombinedFollowup192':'MediaTek PowerVR GE8300 verification','MT8167CombinedFollowup190':'MediaTek PowerVR GE8300 verification','MT8167CombinedFollowup188':'MediaTek PowerVR GE8300 verification','MT8167CombinedFollowup186':'MediaTek PowerVR GE8300 verification','MT8167CombinedFollowup184':'MediaTek GPU359 verification','MT8167CombinedFollowup182':'MediaTek GPU359 verification','MT8167CombinedFollowup180':'MediaTek GPU358 verification','MT8167CombinedFollowup178':'MediaTek GPU357 verification','MT8167CombinedFollowup176':'MediaTek GPU356 verification','MT8167CombinedFollowup174':'MediaTek GPU354 verification','MT8167CombinedFollowup172':'MediaTek GPU353 verification','MT8167CombinedFollowup170':'MediaTek GPU352 verification','MT8167CombinedFollowup168':'MediaTek GPU351 verification','MT8167CombinedFollowup166':'MediaTek GPU349 verification','MT8167CombinedFollowup164':'MediaTek GPU348 MT6630 verification','MT8167CombinedFollowup162':'MediaTek GPU347 verification','MT8167CombinedFollowup160':'MediaTek GPU346 MT6630 Bluetooth verification','MT8167CombinedFollowup158':'MediaTek GPU345 passive waits verification','MT8167CombinedFollowup156':'MediaTek GPU344 durable progress verification','MT8167CombinedFollowup154':'MediaTek matched thermal1117 GPU343 verification','MT8167CombinedFollowup152':'MediaTek GPU342 verification','MT8167CombinedFollowup150':'MediaTek combined features150','MT8167CombinedFollowup148':'MediaTek combined features148','MT8167CombinedFollowup146':'MediaTek combined features146','MT8167CombinedFollowup144':'MediaTek combined features144','MT8167CombinedFollowup142':'MediaTek combined audio142', 'MT8167CombinedFollowup140':'MediaTek combined peripherals140', 'MT8167CombinedFollowup138':'MediaTek combined peripherals138', 'MT8167CombinedFollowup136':'MediaTek GPU335 UMD update', 'MT8167CombinedFollowup134':'MediaTek GPU332 combined update', 'MT8167CombinedFollowup132':'MediaTek RTC AFE readonly followup', 'MT8167CombinedFollowup130':'MediaTek combined followup batch', 'MT8167CombinedFollowup128':'MediaTek combined followup batch', 'MT8167CombinedFollowup126':'MediaTek combined followup batch', 'MT8167CombinedFollowup124':'MediaTek combined followup batch', 'MT8167PeripheralNext122':'MediaTek next peripheral batch', 'MT8167PeripheralBatch':'MediaTek fixed peripheral batch', 'MT8167HardwareProbe': 'MediaTek MT8167 Hardware Probe', 'MT8167EngineProbe': 'MediaTek MT8167 Engine Probe', 'MT8167RenderProbe': 'PowerVR GE8300 Render Probe', 'MT8167RenderStage': 'PowerVR GE8300 Driver Update', 'MT8167RenderSingleGpu': 'PowerVR GE8300 Single GPU Update'}
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
    if selected_service == 'MT8167CombinedFollowup184':
        for phase, before in [('patch','01000000'),('verify','00000000')]:
            audit=report[phase]['state']['post_policy360']
            if audit['before_raw']!=before or audit['after_raw']!='00000000' or audit['equal'] is not True:
                raise ValueError('185 exact one-shot policy360 native proof required')
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
        related_names={'select','device_install','rpc_ss'}
        if selected_service=='MT8167RenderSingleGpu':
            if (not re.fullmatch('[0-9a-f]{64}',report.get('engine_preservation_plan_sha256',''))
                    or report.get('engine_rearm_plan_sha256')
                    or before.get('engine_service')!='dbbe363d2401dfc022606aba010c0e4888c5feab67c748ea5030b0eb3ba484c1'
                    or not re.fullmatch('[0-9a-f]{64}',before.get('engine_ledger',''))):
                raise ValueError('single GPUの既存Engine保持証拠が必要です')
            related_names|={'engine_service','engine_ledger'}
            if report.get('single_gpu_rearm'):
                if not re.fullmatch('[0-9a-f]{64}',before.get('single_gpu_ledger','')) or services[0].get('diagnostic_fingerprint')!=before['single_gpu_ledger']:raise ValueError('single GPU ledger preservation missing')
                related_names.add('single_gpu_ledger')
        if selected_service == 'MT8167CombinedFollowup200':
            p = expected_peripheral_policy
            if not p or p['profile'] != 'combined-features-fresh200-gpu368-v1' or not re.fullmatch('[0-9a-f]{64}',report.get('peripheral_preservation_plan_sha256','')):
                raise ValueError('GPU368 exact fresh200 policy binding required')
            if report.get('peripheral_old_services_preserved') is not True or report.get('engine_rearm_plan_sha256'):
                raise ValueError('GPU368 preservation proof required')
            old = json.loads(before.get('peripheral_old_services','{}'))
            observed = json.loads(before.get('peripheral_captured_values','{}'))
            expected_captured200 = {v['key']+'\\'+v['name']:v for v in p['captured_registry_values']}
            if old != p['service_preservation'] or set(observed) != set(expected_captured200):
                raise ValueError('fresh200 exact service/named-value sets mismatch')
            for k,v in expected_captured200.items():
                if any(observed[k][f] != v[f] for f in ('type','raw_hex')) or observed[k]['bytes'] != len(bytes.fromhex(v['raw_hex'])):
                    raise ValueError('fresh200 named value mismatch')
            if before.get('peripheral_failed149_service_absent') != 'MT8167CombinedFollowup148=absent':
                raise ValueError('Rejected149 must remain absent')
            related_names |= {'peripheral_old_services','peripheral_captured_values','peripheral_failed149_service_absent'}
        if selected_service in ('MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','MT8167CombinedFollowup136','MT8167CombinedFollowup138','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154','MT8167CombinedFollowup156','MT8167CombinedFollowup158','MT8167CombinedFollowup160','MT8167CombinedFollowup162','MT8167CombinedFollowup164','MT8167CombinedFollowup166','MT8167CombinedFollowup168','MT8167CombinedFollowup170','MT8167CombinedFollowup172','MT8167CombinedFollowup174','MT8167CombinedFollowup176','MT8167CombinedFollowup178','MT8167CombinedFollowup180','MT8167CombinedFollowup182','MT8167CombinedFollowup184','MT8167CombinedFollowup186','MT8167CombinedFollowup188','MT8167CombinedFollowup190','MT8167CombinedFollowup192','MT8167CombinedFollowup196','MT8167CombinedFollowup194'):
            if (not re.fullmatch('[0-9a-f]{64}',report.get('peripheral_preservation_plan_sha256',''))
                    or report.get('peripheral_old_services_preserved') is not True
                    or report.get('engine_rearm_plan_sha256')):
                raise ValueError('peripheral専用plan/旧service保持証拠が必要です')
            old=json.loads(before.get('peripheral_old_services','{}'))
            if selected_service in ('MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154','MT8167CombinedFollowup156','MT8167CombinedFollowup158','MT8167CombinedFollowup160','MT8167CombinedFollowup162','MT8167CombinedFollowup164','MT8167CombinedFollowup166','MT8167CombinedFollowup168','MT8167CombinedFollowup170','MT8167CombinedFollowup172','MT8167CombinedFollowup174','MT8167CombinedFollowup176','MT8167CombinedFollowup178','MT8167CombinedFollowup180','MT8167CombinedFollowup182','MT8167CombinedFollowup184','MT8167CombinedFollowup186','MT8167CombinedFollowup188','MT8167CombinedFollowup190','MT8167CombinedFollowup192','MT8167CombinedFollowup196','MT8167CombinedFollowup194') and set(old)!=set(['MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','Mt6392Keys','MT8167CombinedFollowup136','MT8167CombinedFollowup140','MT8167CombinedFollowup142','MT8167CombinedFollowup144']):raise ValueError('Exact18 old service snapshots required')
            if selected_service=='MT8167CombinedFollowup144' and set(old)!=set(['MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','MT8167CombinedFollowup134','Mt6392Keys','MT8167CombinedFollowup136','MT8167CombinedFollowup140','MT8167CombinedFollowup142']):raise ValueError('Exact17 old service snapshots required')
            if selected_service not in ('MT8167CombinedFollowup144','MT8167CombinedFollowup146','MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154','MT8167CombinedFollowup156','MT8167CombinedFollowup158','MT8167CombinedFollowup160','MT8167CombinedFollowup162','MT8167CombinedFollowup164','MT8167CombinedFollowup166','MT8167CombinedFollowup168','MT8167CombinedFollowup170','MT8167CombinedFollowup172','MT8167CombinedFollowup174','MT8167CombinedFollowup176','MT8167CombinedFollowup178','MT8167CombinedFollowup180','MT8167CombinedFollowup182','MT8167CombinedFollowup184','MT8167CombinedFollowup186','MT8167CombinedFollowup188','MT8167CombinedFollowup190','MT8167CombinedFollowup192','MT8167CombinedFollowup196','MT8167CombinedFollowup194') and set(old)!=(set(['MT8167EngineProbe', 'MT8167RenderSingleGpu', 'MT8167RenderStage', 'MT8167DeviceInstall', 'RpcSs', 'MT8167PeripheralBatch', 'MT8167PeripheralNext122', 'MT8167CombinedFollowup124', 'MT8167CombinedFollowup126', 'MT8167CombinedFollowup128', 'MT8167CombinedFollowup130', 'MT8167CombinedFollowup132', 'MT8167CombinedFollowup134', 'Mt6392Keys', 'MT8167CombinedFollowup136', 'MT8167CombinedFollowup140']) if selected_service=='MT8167CombinedFollowup142' else ({'MT8167RenderStage', 'MT8167RenderSingleGpu', 'MT8167EngineProbe', 'MT8167CombinedFollowup134', 'MT8167CombinedFollowup126', 'MT8167CombinedFollowup132', 'MT8167PeripheralNext122', 'MT8167CombinedFollowup124', 'MT8167DeviceInstall', 'MT8167CombinedFollowup136', 'MT8167PeripheralBatch', 'Mt6392Keys', 'RpcSs', 'MT8167CombinedFollowup130', 'MT8167CombinedFollowup128'} if selected_service in ('MT8167CombinedFollowup138','MT8167CombinedFollowup140') else ({'MT8167CombinedFollowup134', 'MT8167PeripheralBatch', 'MT8167CombinedFollowup132', 'MT8167CombinedFollowup124', 'MT8167EngineProbe', 'RpcSs', 'MT8167DeviceInstall', 'Mt6392Keys', 'MT8167RenderStage', 'MT8167PeripheralNext122', 'MT8167CombinedFollowup128', 'MT8167CombinedFollowup130', 'MT8167RenderSingleGpu', 'MT8167CombinedFollowup126'} if selected_service=='MT8167CombinedFollowup136' else ({'MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','MT8167CombinedFollowup132','Mt6392Keys'} if selected_service=='MT8167CombinedFollowup134' else ({'MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs','MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','MT8167CombinedFollowup130','Mt6392Keys'} if selected_service=='MT8167CombinedFollowup132' else ({'MT8167EngineProbe','MT8167RenderSingleGpu','MT8167RenderStage','MT8167DeviceInstall','RpcSs'}|({'MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','MT8167CombinedFollowup128','Mt6392Keys'} if selected_service=='MT8167CombinedFollowup130' else ({'MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124','MT8167CombinedFollowup126','Mt6392Keys'} if selected_service=='MT8167CombinedFollowup128' else ({'MT8167PeripheralBatch','MT8167PeripheralNext122','MT8167CombinedFollowup124'} if selected_service=='MT8167CombinedFollowup126' else ({'MT8167PeripheralBatch','MT8167PeripheralNext122'} if selected_service=='MT8167CombinedFollowup124' else ({'MT8167PeripheralBatch'} if selected_service=='MT8167PeripheralNext122' else set()))))))))))) :
                raise ValueError('peripheral旧5service snapshot不一致')
            related_names.add('peripheral_old_services')
            if selected_service in ('MT8167CombinedFollowup148','MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154','MT8167CombinedFollowup156','MT8167CombinedFollowup158','MT8167CombinedFollowup160','MT8167CombinedFollowup162','MT8167CombinedFollowup164','MT8167CombinedFollowup166','MT8167CombinedFollowup168','MT8167CombinedFollowup170','MT8167CombinedFollowup172','MT8167CombinedFollowup174','MT8167CombinedFollowup176','MT8167CombinedFollowup178','MT8167CombinedFollowup180','MT8167CombinedFollowup182','MT8167CombinedFollowup184','MT8167CombinedFollowup186','MT8167CombinedFollowup188','MT8167CombinedFollowup190','MT8167CombinedFollowup192','MT8167CombinedFollowup196','MT8167CombinedFollowup194'):
                values=json.loads(before.get('peripheral_captured_values','{}'))
                if len(values)!={'MT8167CombinedFollowup196':125,'MT8167CombinedFollowup194':120,'MT8167CombinedFollowup192':115,'MT8167CombinedFollowup190':110,'MT8167CombinedFollowup188':105,'MT8167CombinedFollowup186':100,'MT8167CombinedFollowup184':95,'MT8167CombinedFollowup182':90,'MT8167CombinedFollowup180':85,'MT8167CombinedFollowup178':80,'MT8167CombinedFollowup176':75,'MT8167CombinedFollowup174':70,'MT8167CombinedFollowup172':65,'MT8167CombinedFollowup170':60,'MT8167CombinedFollowup168':55,'MT8167CombinedFollowup166':50,'MT8167CombinedFollowup164':45,'MT8167CombinedFollowup162':40,'MT8167CombinedFollowup160':35,'MT8167CombinedFollowup158':30,'MT8167CombinedFollowup156':25,'MT8167CombinedFollowup154':20,'MT8167CombinedFollowup152':15}.get(selected_service,10) or any(not re.fullmatch(r'([0-9a-f]{2}){1,4096}',v.get('raw_hex','')) for v in values.values()):raise ValueError('18 hashと別に旧146/BT10named valuesが必要')
                related_names.add('peripheral_captured_values')
                if selected_service in ('MT8167CombinedFollowup150','MT8167CombinedFollowup152','MT8167CombinedFollowup154','MT8167CombinedFollowup156','MT8167CombinedFollowup158','MT8167CombinedFollowup160','MT8167CombinedFollowup162','MT8167CombinedFollowup164','MT8167CombinedFollowup166','MT8167CombinedFollowup168','MT8167CombinedFollowup170','MT8167CombinedFollowup172','MT8167CombinedFollowup174','MT8167CombinedFollowup176','MT8167CombinedFollowup178','MT8167CombinedFollowup180','MT8167CombinedFollowup182','MT8167CombinedFollowup184','MT8167CombinedFollowup186','MT8167CombinedFollowup188','MT8167CombinedFollowup190','MT8167CombinedFollowup192','MT8167CombinedFollowup196','MT8167CombinedFollowup194'):
                    if before.get('peripheral_failed149_service_absent')!='MT8167CombinedFollowup148=absent':raise ValueError('Rejected149 must remain unarmed/absent')
                    related_names.add('peripheral_failed149_service_absent')
            if selected_service in ('MT8167CombinedFollowup124','MT8167CombinedFollowup126'):
                if not re.fullmatch('[0-9a-f]{64}',before.get('followup_keypad','')):raise ValueError('Pinned unbound keypad fingerprint required')
                related_names.add('followup_keypad')
        if set(before) != related_names or before != after:
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
        codec=report.get('build_defines',{}).get('A04_ENGINE_CODEC_MSDC')=='1'
        if codec:audio='mt8167-capture-probe.exe'
        brightness=report.get('build_defines',{}).get('A04_ENGINE_BRIGHTNESS_CLIENT')=='1'
        expected_clients=[audio,'ge8300-d3d-clear.exe'] if codec else ['mt6630-wlan-probe.exe',audio]+(['mt8167-brightness-native.exe'] if brightness else [])
        observe=report.get('build_defines',{}).get('A04_ENGINE_OBSERVE_ONLY')=='1'
        if codec and (brightness or report.get('deferred_update',{}).get('transaction') not in ('codec1026-msdc-quiet-render313-v3','codec-render-observe-fresh92-v1')):raise ValueError('codec/eMMC専用profile不一致')
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
            if codec:
                from codec_msdc_transaction import TARGETS
            else:
                from engine_deferred_policy import TARGETS
            has_gpu='gpu' in deferred.get('packages',{})
            has_sd='sd' in deferred.get('packages',{})
            expected_ids=[x[0] for x in TARGETS.values()]+(['ACPI\\MOU0006'] if has_gpu else [])+(['ACPI\\MOU0013'] if has_sd else [])
            if observe:
                from codec_observe import check_service
                check_service(report)
            elif (deferred.get('mode') != 'engine_fixed_deferred_v1' or deferred.get('schema') != 1
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
            if codec:raise ValueError('codec専用policyが必要です')
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
