#!/usr/bin/env python3
"""fresh196と既署名ABI3 tupleから専用USB導入bundleを作る。実機操作なし。"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

FRESH = '7e6c1316961a1dc0807595552434488ff0efa43d3f43629bea8aef599a2ff8d2'
BOOT = '18658914-6059-4b77-aa12-97f21f8e18e3'
NATIVE = '552d78c6bdc9c77e0b2d4075d5d499c3bc32207fe25727edd9552586a0acc23f'
SOURCE = '6741f2a1cac2cb6622c3b1d49bd2ac118c76471f'
TUPLE = '15433a4037b4d739f9525d87967f5782e6576c337ac4c2d6d0ada69d6deeb55f'
IDENTITIES = {
    'parent': ('ACPI\\BEN0001', 'Mt8167Shared', '{4d36e97d-e325-11ce-bfc1-08002be10318}'),
    'legacy': ('MT8167\\USB20', 'mshidkmdf', '{745a17a0-74d3-11d0-b6fe-00a0c90f57da}'),
    'hcd': ('MT8167\\USBPORT20', 'MT8167MUSBHcd', '{36fc9e60-c465-11cf-8056-444553540000}'),
}
def require(value, message):
    if not value:
        raise ValueError(message)
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))
def below(root, path):
    result = (root / path).resolve(strict=True)
    require(result.is_relative_to(root), '作業root外の入力')
    return result

def prepare(root, output):
    root = root.resolve(strict=True)
    output = output.resolve()
    require(output.is_relative_to(root) and not output.exists(), 'root内の新規出力が必要')
    fresh_path = below(root, 'port/xnu/out/private-windows-rt-prebackup-run196/manifest.json')
    fresh = read(fresh_path)
    require(sha(fresh_path) == FRESH and fresh['recovery_boot_id'] == BOOT and
            fresh['status'] == 'captured-unmounted' and fresh['read_only'] and
            fresh['unmounted'] and not fresh['errors'], 'fresh196原本不一致')
    original = {x['source_path']: x for x in fresh['files']}
    for name in ('SYSTEM', 'SYSTEM.LOG1', 'SYSTEM.LOG2'):
        f = original['Windows/System32/config/' + name]
        require(sha(below(root, fresh_path.parent / f['backup_path'])) == f['sha256'], 'SYSTEM原本変更')
    receipt_path = below(root, 'port/xnu/out/private-windows-rt-combined-run195-registry-post/receipt.json')
    receipt = read(receipt_path)
    require(sha(receipt_path) == NATIVE and receipt['source_manifest_sha256'] == FRESH and receipt['originals_unchanged'] and
            receipt['full_service_preservation_equal'] and not receipt['errors'], 'native196 receipt不一致')
    baseline = read(below(root, 'port/xnu/out/windows-rt-combined195-fresh194/service/peripheral-policy.json'))
    current = {}
    for role, version in [('parent', '2026.11.10.1120'), ('legacy', '2026.11.8.1010')]:
        hwid, service, cls = IDENTITIES[role]
        node = receipt['nodes'][hwid]
        require(node['present'] and len(node['instances']) == 1, 'baseline PDOが一意でない')
        node = node['instances'][0]
        require(node['selected']['DriverVersion']['value'] == version and
                node['values']['Service']['value'].lower() == service.lower() and
                node['values']['ClassGUID']['value'].lower() == cls, 'baseline選択tuple不一致')
        current[role] = dict(hwid=hwid, service=service, class_guid=cls, version=version,
                             instance=node['instance'], inf=node['selected']['InfPath']['value'])
    require(receipt['nodes'][IDENTITIES['legacy'][0]]['instances'][0]['values']['LowerFilters']['value'] ==
            ['A04Usb20Keyboard'], '旧keyboard filterが別物')
    package_root = below(root, 'port/xnu/out/windows-rt-musb1121-hcd1019-signed')
    manifest = read(package_root / 'signed-tuple-manifest.json')
    review = read(package_root / 'main-artifact-review.json')
    require(sha(package_root / 'signed-tuple-manifest.json') == TUPLE == review['manifest_sha256'] and
            manifest['source_commit'] == SOURCE and manifest['owner_ABI'] == 3 and
            manifest['parent_version'] == '2026.11.10.1121' and
            manifest['hcd_version'] == '2026.11.10.1019' and
            manifest['source_ON_requested'] is False and review['ARMNT_and_import_manifest_match'] is True and
            review['files'] == manifest['files'], '既署名1121/1019 ABI3 tuple不一致')
    require(any(x['kind'] == 'parent' and '-DA04_SHARED_USBPORT_CHILD=1' in x['args']
                for x in manifest['compiled_objects']), 'dynamic USBPORT PDO buildではない')
    copies = []
    packages = {}
    for role in ('parent', 'hcd'):
        names = {}
        for f in manifest['files']:
            if not f['path'].startswith(role + '/'):
                continue
            source = below(root, package_root / 'tuple/package' / f['path'])
            require(sha(source) == f['sha256'], '候補leafのbyte不一致')
            destination = 'candidate/' + f['path']
            names[source.suffix[1:]] = destination
            copies.append((source, destination, f['sha256']))
        require(set(names) == {'sys', 'inf', 'cat'}, '候補trio不足')
        packages[role] = dict(names=names, hwid=IDENTITIES[role][0], service=IDENTITIES[role][1],
                              class_guid=IDENTITIES[role][2], version=manifest[role + '_version'])
    rollback = {}
    for role, oldrole in [('parent', 'parent'), ('legacy', 'usb')]:
        entry = baseline['packages'][oldrole]
        names = {}
        for kind, name in entry['names'].items():
            source = below(root, Path(entry['directory']) / name)
            require(sha(source) == entry['hashes'][kind], 'rollback署名trioの変更')
            if kind == 'sys':
                f = original['Windows/System32/Drivers/' + name]
                require(f['sha256'] == entry['hashes'][kind] and
                        sha(below(root, fresh_path.parent / f['backup_path'])) == f['sha256'],
                        '実配置SYSとrollback packageが不一致')
            destination = 'rollback/' + role + '/' + name
            names[kind] = destination
            copies.append((source, destination, entry['hashes'][kind]))
        rollback[role] = dict(current[role], names=names)
    repo = Path(__file__).resolve().parents[2]
    for name in ('UsbAbi3Pnp.cs', 'install-usb1121-first.ps1'):
        source = repo / 'tools/usb' / name
        copies.append((source, 'recipe/' + name, sha(source)))
    plan = dict(schema='MT8167_USB_ABI3_FIRST_INSTALL_V1', capture_sha256=FRESH,
                recovery_boot_id=BOOT, native_receipt_sha256=sha(receipt_path), owner_ABI=3,
                signed_tuple_manifest_sha256=TUPLE, candidate=packages, baseline=current,
                rollback=rollback, old_filter='A04Usb20Keyboard', tool_signer=manifest['signer'],
                files={name: h for _, name, h in copies},
                actions=['stage-both', 'disable-legacy-devnode', 'DIF-parent', 'restart-parent',
                         'reenumerate-parent', 'observe-real-PDO-auto-bind'],
                root_PDO_creation=False, force_host=False, source_ON=False,
                hardware_verified=False, normal_install_performed=False)
    output.mkdir(parents=True)
    for source, name, _ in copies:
        dest = output / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, dest)
    (output / 'plan.json').write_text(json.dumps(plan, ensure_ascii=False, indent=2) + '\n')
    (output / 'plan.sha256').write_text(sha(output / 'plan.json') + '\n')
    return plan

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--workspace', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    a = ap.parse_args()
    prepare(a.workspace, a.out)
    print('USB_ABI3_BUNDLE_VERIFIED_NO_INSTALLATION')
