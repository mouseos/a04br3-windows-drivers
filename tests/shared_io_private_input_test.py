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
