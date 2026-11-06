"""Private input boundary regression; synthetic base block, no real hive."""
import argparse
import copy
import importlib.util
import json
import struct
import tempfile
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument('--scratch', type=Path, required=True)
args = ap.parse_args()
repo = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('private_inputs', repo / 'tools/winpe/private_hardware_probe_input.py')
inputs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(inputs)

with tempfile.TemporaryDirectory(dir=args.scratch, prefix='synthetic-system-contract-') as temp:
    root = Path(temp)
    (root / 'candidate').mkdir()
    system = root / 'candidate/SYSTEM'
    raw = bytearray(4096)
    raw[:4] = b'regf'
    struct.pack_into('<II', raw, 4, 10, 10)
    struct.pack_into('<IIII', raw, 20, 1, 5, 0, 1)
    checksum = 0
    for value in struct.unpack('<127I', raw[:508]):
        checksum ^= value
    struct.pack_into('<I', raw, 508, checksum or 1)
    system.write_bytes(raw)
    digest = inputs.sha(system)
    values = {'Type': ('DWord', 16), 'Start': ('DWord', 2), 'ErrorControl': ('DWord', 1),
              'ImagePath': ('ExpandString', '"C:\\Windows\\System32\\MT8167HardwareProbe.exe"'),
              'ObjectName': ('String', 'LocalSystem'),
              'DisplayName': ('String', 'MediaTek MT8167 Hardware Probe'),
              'DependOnService': ('MultiString', ['DeviceInstall', 'RpcSs'])}
    fingerprints = {name: {'synthetic': True} for name in ('select', 'device_install', 'rpc_ss')}
    state = {'status': 'success', 'load_rc': 0, 'unload_rc': 0, 'errors': [],
             'whole_system_audit': False, 'audit_scope': 'related direct values only',
             'services': [{'name': 'MT8167HardwareProbe', 'values': {
                 name: {'kind': value[0], 'value': value[1]} for name, value in values.items()}}],
             'related_fingerprints_before': fingerprints, 'related_fingerprints_after': fingerprints}
    report = {'status': 'verified-clean-system-services-only', 'whole_system_audit': False,
              'audit_scope': 'related direct values plus seven service values',
              'services': ['MT8167HardwareProbe'], 'candidate': {'path': 'candidate/SYSTEM', 'sha256': digest},
              'patch': {'finished': True, 'exit_code': 0, 'state': state},
              'verify': {'finished': True, 'exit_code': 0, 'state': state},
              'source_files': {name: {'sha256': '1' * 64, 'bytes': 4096}
                               for name in ('SYSTEM', 'SYSTEM.LOG1', 'SYSTEM.LOG2')}}
    backup = {'files': [{'source_path': 'Windows/System32/config/' + name,
                         'sha256': '1' * 64, 'bytes': 4096} for name in report['source_files']]}
    manifest = root / 'result.json'
    manifest.write_text(json.dumps(report))
    assert inputs.prepared_system_check(system, manifest, digest, root, backup)['sha256'] == digest
    # Explicit Engine identity must match the native selected-service report.
    engine_report = copy.deepcopy(report)
    engine_report['services'] = ['MT8167EngineProbe']
    for phase in ('patch', 'verify'):
        service = engine_report[phase]['state']['services'][0]
        service['name'] = 'MT8167EngineProbe'
        service['values']['ImagePath']['value'] = '"C:\\Windows\\System32\\MT8167EngineProbe.exe"'
        service['values']['DisplayName']['value'] = 'MediaTek MT8167 Engine Probe'
    manifest.write_text(json.dumps(engine_report))
    assert inputs.prepared_system_check(system, manifest, digest, root, backup, 'MT8167EngineProbe')['selected_service'] == 'MT8167EngineProbe'
    try:
        inputs.prepared_system_check(system, manifest, digest, root, backup)
    except ValueError:
        pass
    else:
        raise AssertionError('Engine candidate accepted as legacy tone service')
    manifest.write_text(json.dumps(report))
    flat = root / 'SYSTEM'
    flat.write_bytes(system.read_bytes())
    assert inputs.prepared_system_check(flat, manifest, digest, root, backup)['relocated_after_native_preparation'] is True
    for case in ('failed', 'wrong-service', 'wrong-hash', 'wrong-value', 'outside-root'):
        bad = copy.deepcopy(report)
        if case == 'failed': bad['status'] = 'failed-do-not-deploy'
        if case == 'wrong-service': bad['services'] = ['MT8167DeviceInstall']
        if case == 'wrong-hash': bad['candidate']['sha256'] = '0' * 64
        if case == 'wrong-value': bad['verify']['state']['services'][0]['values']['Start']['value'] = 4
        manifest.write_text(json.dumps(bad))
        try:
            inputs.prepared_system_check(system, manifest, digest,
                                         root / 'candidate' if case == 'outside-root' else root, backup)
        except ValueError:
            pass
        else:
            raise AssertionError(case)

template = (repo / 'tools/winpe/prepare-mt8167-hardware-probe.cmd.in').read_text()
assert 'reg.exe' not in template.lower()
bulk = (repo / 'tools/winpe/run-mt8167-shared-io-update.cmd.in').read_text()
assert bulk.index('@PROBE_PREPARE@') < bulk.index('call :adddriver PARENT')
certificates = {'package_clients': b'\x30old-certificate', 'hardware_probe': b'\x30new-certificate'}
trust = inputs.certificate_trust_commands(certificates)
adds = [line for line in trust if line.startswith('reg.exe add ')]
assert len(adds) == 4
for cert in certificates.values():
    thumb = inputs.hashlib.sha1(cert).hexdigest().upper()
    assert sum(thumb in line for line in adds) == 2
print('Private input boundary and copy-before-DISM contract passed; synthetic data only.')

engine_template = (repo / 'tools/winpe/prepare-mt8167-engine-probe.cmd.in').read_text()
assert 'reg.exe' not in engine_template.lower()
assert 'MT8167HardwareProbe' not in engine_template and 'first-playback' not in engine_template
assert 'MT8167EngineProbe\\attempted.bin' in engine_template

# A newly compiled Engine service is mandatory: stale launch-only manifests fail.
with tempfile.TemporaryDirectory(dir=args.scratch, prefix='engine-probe-contract-') as temp:
    root = Path(temp)
    exe = root / 'MT8167EngineProbe.exe'
    exe.write_bytes('mt8167-audio-engine-probe.exe'.encode('utf-16le'))
    cert = root / 'public.cer'
    cert.write_bytes(b'\x30synthetic-contract-certificate')
    manifest = root / 'hardware-probe-manifest.json'
    report = {'audio_mode': 'engine_pcm_retained', 'engine_pcm_compatible': True,
              'fixed_clients': ['mt6630-wlan-probe.exe', 'mt8167-audio-engine-probe.exe'],
              'build_defines': {'A04_HARDWARE_PROBE_ENGINE_PCM': '1'},
              'service': 'MT8167EngineProbe', 'service_destination': 'System32\\MT8167EngineProbe.exe',
              'client_directory': 'System32\\MT8167EngineProbe\\clients',
              'log_directory': 'System32\\MT8167EngineProbe\\logs',
              'marker_directory': 'System32\\MT8167EngineProbe', 'signing_requested': True,
              'source_dirty': False, 'source_commit': '0' * 40,
              'signing_thumbprint': inputs.hashlib.sha1(cert.read_bytes()).hexdigest().upper(),
              'files': [{'name': exe.name, 'sha256': inputs.sha(exe)}],
              'scm_start': 2, 'scm_arguments': '', 'audio_interface': 11,
              'pnp_rebind': {'api': 'UpdateDriverForPlugAndPlayDevicesW', 'flags': 5,
                  'fixed_hardware_ids': ['MT8167\\AFE', 'MT8167\\USB20', 'A04BR3\\MT6630SDIO'],
                  'directory': 'System32\\MT8167EngineProbe\\drivers', 'maximum_calls_per_id': 1,
                  'parent_updated': False, 'reboot_required_stops_clients': True, 'catalog_signature_required': True}}
    manifest.write_text(json.dumps(report))
    assert inputs.probe_check(exe, manifest, cert, root, 'EnginePcm')['pnp_rebind']['maximum_calls_per_id'] == 1
    del report['pnp_rebind']
    manifest.write_text(json.dumps(report))
    try:
        inputs.probe_check(exe, manifest, cert, root, 'EnginePcm')
    except ValueError:
        pass
    else:
        raise AssertionError('launch-only Engine service accepted without fixed PnP contract')
print('Engine service manifest boundary passed; signatures/hardware not simulated as verified.')
