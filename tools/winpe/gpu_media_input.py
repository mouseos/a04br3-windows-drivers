"""固定GPU試験候補の配置だけを許可する。任意コマンドは受け取らない。"""
from engine_io_package import SignedPackage, ENGINE_CERT
from private_hardware_probe_input import sha
import importlib.util
from pathlib import Path

_spec = importlib.util.spec_from_file_location('fixed_gpu_policy', Path(__file__).with_name('prepare-gpu-provider-policy.py'))
_policy = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_policy)
SOURCE, FIXED_HASHES = _policy.SOURCE, _policy.FIXED_HASHES

INSTALLER_SOURCE = 'd929cc8f3dbd537a582a589aecd500ec1f38300b'
INSTALLER_SHA = '42ff2088c59fdb6b82a4f8a8119604409976c7b7ed38ea137066ca6782d71c22'


def gpu_input(directory, root):
    package = SignedPackage(directory, root)
    record = package.json('installer-manifest.json')
    policy = package.json('policy-manifest.json')
    if (package.record.get('source_commit') != INSTALLER_SOURCE
            or record.get('source_commit') != INSTALLER_SOURCE
            or record.get('source_dirty') is not False or record.get('signed') is not True
            or record.get('service') != 'MT8167DeviceInstall'
            or record.get('hardware_id') != 'ACPI\\MOU0006'
            or record.get('mode') != 'fixed_gpu_provider_deferred_install'
            or record.get('candidate') != policy or policy.get('candidate_source_commit') != SOURCE
            or policy.get('driver_ver') != '11/07/2026,0.0.2.1'):
        raise ValueError('固定GPU installer/policyの出自不一致')
    package.certificate(ENGINE_CERT)
    exe = package.file('MT8167DeviceInstall.exe')
    if sha(exe) != INSTALLER_SHA or record['exe_sha256'].lower() != INSTALLER_SHA:
        raise ValueError('固定署名installerの不一致')
    names = {'provider-package-manifest.json', *FIXED_HASHES}
    if set(policy['files']) != names:
        raise ValueError('GPU配置対象は固定5fileだけです')
    files = {'MT8167DeviceInstall.exe': exe}
    for name in sorted(names):
        path = package.file('provider/' + name)
        if sha(path) != policy['files'][name].lower() or (name in FIXED_HASHES and sha(path) != FIXED_HASHES[name]):
            raise ValueError('GPU配置fileのhash不一致: ' + name)
        files['provider/' + name] = path
    return files, {'installer_source': INSTALLER_SOURCE, 'candidate_source': SOURCE,
                   'installer_manifest_sha256': sha(package.file('installer-manifest.json')),
                   'policy_manifest_sha256': sha(package.file('policy-manifest.json')),
                   'files': {name: sha(path) for name, path in files.items()},
                   'target_authenticode_verified_here': False}
