"""固定MOU0006の署名済み新旧候補だけを配置する。任意コマンドは受け取らない。"""
import hashlib
import importlib.util
from pathlib import Path
import re
import subprocess
from engine_io_package import SignedPackage, ENGINE_CERT
from private_hardware_probe_input import sha

_spec = importlib.util.spec_from_file_location('fixed_gpu_policy', Path(__file__).with_name('prepare-gpu-provider-policy.py'))
_policy = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_policy)


def gpu_input(directory, root, baseline_directory, candidate_directory):
    checkout = Path(__file__).resolve().parents[2]
    for supplied in (directory, baseline_directory, candidate_directory):
        if not supplied.resolve(strict=True).is_relative_to(root.resolve(strict=True)):
            raise ValueError('GPU入力は作業root内が必要です')
    baseline = _policy.load_package(checkout, baseline_directory.resolve(strict=True))
    candidate = _policy.load_package(checkout, candidate_directory.resolve(strict=True), candidate=True)
    _policy.validate_transition(baseline, candidate)
    transition = hashlib.sha256((candidate['source'] + baseline['hashes']['mt8167-ge8300.sys'] +
        candidate['hashes']['mt8167-ge8300.sys'] + candidate['hashes']['mt8167-ge8300.inf']).encode('ascii')).hexdigest()
    suffix = '\\MT8167GpuUpdate-' + candidate['source'] + '-' + transition
    target = 'C:\\Windows\\System32' + suffix
    package = SignedPackage(directory, root)
    record = package.json('installer-manifest.json')
    policy = package.json('policy-manifest.json')
    source = package.record.get('source_commit', '')
    if (not re.fullmatch('[0-9a-f]{40}', source) or record.get('source_commit') != source
            or record.get('source_dirty') is not False or record.get('signed') is not True
            or record.get('service') != 'MT8167DeviceInstall' or record.get('hardware_id') != 'ACPI\\MOU0006'
            or record.get('mode') != 'fixed_gpu_provider_deferred_install'
            or record.get('policy_schema') != 2 or policy.get('schema_version') != 2
            or record.get('candidate') != policy or policy.get('candidate_source_commit') != candidate['source']
            or policy.get('driver_ver') != candidate['driver_ver']
            or policy.get('baseline_source_commit') != baseline['source']
            or policy.get('baseline_driver_ver') != baseline['driver_ver']
            or policy.get('baseline_sys_sha256') != baseline['hashes']['mt8167-ge8300.sys']
            or policy.get('baseline_signed_manifest_sha256') != baseline['signed_sha256']
            or policy.get('source_signed_manifest_sha256') != candidate['signed_sha256']
            or policy.get('source_build_config_sha256') != candidate['verified_hashes']['build-config.json']
            or any(part.get('data_directory') != target or part.get('data_directory_suffix') != suffix
                   or part.get('transition_id') != transition for part in (record, policy))):
        raise ValueError('固定GPU installer/policyの新旧候補・配置先が不一致です')
    subprocess.run(['git', '-C', str(checkout), 'cat-file', '-e', source + '^{commit}'], check=True)
    package.certificate(ENGINE_CERT)
    if package.record.get('signing_thumbprint') != ENGINE_CERT:
        raise ValueError('署名installerの証明書出自不一致')
    exe = package.file('MT8167DeviceInstall.exe')
    if record['exe_sha256'].lower() != sha(exe):
        raise ValueError('署名installerの実hash不一致')
    if sha(package.file('gpu-provider-candidate.h')) != policy.get('policy_sha256'):
        raise ValueError('署名installer生成policyの不一致')
    names = {'provider-package-manifest.json', *_policy.FILES}
    if set(policy['files']) != names:
        raise ValueError('GPU配置対象は固定5fileだけです')
    files = {'MT8167DeviceInstall.exe': exe}
    for name in sorted(names):
        path = package.file('provider/' + name)
        if sha(path) != policy['files'][name].lower() or sha(path) != candidate['verified_hashes'][name]:
            raise ValueError('GPU配置fileのhash不一致: ' + name)
        files['provider/' + name] = path
    return files, {'installer_source': source, 'candidate_source': candidate['source'],
                   'baseline_source': baseline['source'], 'baseline_sys_sha256': baseline['hashes']['mt8167-ge8300.sys'],
                   'data_directory': target, 'transition_id': transition,
                   'installer_manifest_sha256': sha(package.file('installer-manifest.json')),
                   'policy_manifest_sha256': sha(package.file('policy-manifest.json')),
                   'files': {name: sha(path) for name, path in files.items()},
                   'target_authenticode_verified_here': False}
