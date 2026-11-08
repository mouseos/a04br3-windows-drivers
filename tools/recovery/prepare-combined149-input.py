#!/usr/bin/env python3
"""fresh148原本とnative actualからrun149の採用入力を固定する。媒体発火は行わない。"""
import argparse
import hashlib
import json
from pathlib import Path

FRESH = '38d60c8d9f84a08e0c0284505fee17b0e3b32b56060a6a030643bd78f671f6df'
BOOT = '5b5b69d3-70d1-4100-927b-e29eb8bd3026'
BASE = Path('port/xnu/out/private-windows-rt-prebackup-run148')
NATIVE = Path('port/xnu/out/private-windows-rt-combined-run147-registry-post/receipt.json')
LOGS = Path('port/gsi/logs/ios-feasibility')


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def read(p):
    return json.loads(p.read_text(encoding='utf-8-sig'))


def prepare(root):
    pins = {}
    def path(rel):
        p = (root / rel).resolve(strict=True)
        require(p.is_relative_to(root), 'ルート外の入力')
        return p
    def pin(rel):
        p = path(rel)
        pins[str(p.relative_to(root))] = digest(p)
        return p
    fp = pin(BASE / 'manifest.json')
    require(digest(fp) == FRESH, 'fresh148 manifest不一致')
    fm = read(fp)
    require(fm['status'] == 'captured-unmounted' and fm['read_only'] and fm['unmounted']
            and not fm['errors'] and fm['recovery_boot_id'] == BOOT, 'Recovery原本の資格不一致')
    require(len(fm['files']) == 41, '原41が必要')
    files = {}
    for f in fm['files']:
        p = pin(BASE / f['backup_path'])
        require(p.stat().st_size == f['bytes'] and digest(p) == f['sha256'], '原本改変')
        require(f['source_path'] not in files, '重複原本')
        files[f['source_path']] = f
    receipt = read(pin(NATIVE))
    require(receipt['source_manifest_sha256'] == FRESH and not receipt['errors']
            and receipt['originals_unchanged'] and receipt['full_service_preservation_equal']
            and all(receipt[k] == 0 for k in ('load_rc', 'unload_rc', 'privilege_restore_error')),
            'native原本/cleanup不一致')
    for name, h in receipt['source_files'].items():
        require(files['Windows/System32/config/' + name]['sha256'] == h, 'native hive不一致')
    selected = read(pin(LOGS / 'windows-rt-fresh148-selected-drivers.json'))
    require(len(selected) == 14, '実14instancesが必要')
    for hwid, node in receipt['nodes'].items():
        for instance in node.get('instances', []):
            key = hwid + '\\' + instance['instance']
            require(key in selected, 'native instance未収録')
            for k, v in instance.get('selected', {}).items():
                if k in selected[key]:
                    require(selected[key][k] == v['value'], 'native選択不一致')
    bt = receipt['nodes']['ROOT\\MT6630COMBO']
    require(bt['present'] and len(bt['instances']) == 1
            and bt['instances'][0]['instance'] == 'SYSTEM\\0001'
            and not receipt['nodes']['A04BR3\\MT6630HCI']['instances'], 'BT actual変化')
    baseline = read(pin(LOGS / 'windows-rt-fresh148-baseline-client-files.json'))
    require(baseline['fresh_manifest_sha256'] == FRESH and baseline['capture_number'] == 148
            and len(baseline['files']) == 65 and not baseline['missing_prior_files']
            and baseline['client_pid'] == 2660 and baseline['candidate'] == 341,
            'GPUログ65/PID2660/candidate341が必要')
    for name, h in baseline['files'].items():
        require(Path(name).name == name, 'ログ名不正')
        require(digest(pin(Path(baseline['source_directory']) / name)) == h, 'ログ改変')
    draft = read(pin(Path('port/xnu/out/windows-rt-combined148-peripheral/draft-profile.json')))
    msdc = draft['candidate_packages']['msdc110']
    require(msdc['source_commit'] == '47f6e233610d6f8d1a5858d441aa1b80f8611743', 'MSDC110のみ')
    for ref in ('manifest', 'producer_pins', 'main_seal'):
        require(digest(pin(msdc[ref]['path'])) == msdc[ref]['sha256'], 'MSDC seal不一致')
    for f in msdc['files']:
        require(digest(pin(Path(msdc['directory']) / f['path'])) == f['sha256'], 'MSDC署名member不一致')
    parentdir = Path('port/xnu/out/mt8167-thermal-run145-rootfix/parent1116')
    parent = read(pin(parentdir / 'signed-manifest.json'))
    require(parent['signed'] and parent['source_commit'] == '49bf368d9fc746fe9c78bc045d8aa390968cab8d'
            and parent['hp_extension_abi_version'] == 2 and parent['hp_extension_abi_build'] == 1107
            and parent['signer_thumbprint'] == 'BCBF6C456C6F0DD0818434763B189488FBF13570', '親1116/HPv2 ABI不一致')
    for f in parent['files']:
        require(digest(pin(parentdir / f['path'])) == f['sha256'], '親1116署名member不一致')
    leafsrc = read(pin(Path('port/xnu/out/mt8167-thermal-run145-rootfix/leaf-final-source-pin.json')))
    require(leafsrc['source'] == '740800733fe9741023038026a717fd54d43cd7c3', 'leaf1.10 source不一致')
    services = dict(receipt['old_services'])
    # previous combined146 is outside old_services; preserve its actual values explicitly.
    previous = receipt['new_service']
    require(previous['present'] and previous['values']['Start']['value'] == 4, '前サービス停止状態が必要')
    require(receipt['old_gpu_service']['Start']['value'] == 4, '旧GPU自動実行禁止')
    changed = {'mt8167-msdc.sys', 'mt8167-shared.sys', 'mt8167-thermal.sys'}
    return {
        'schema': 1, 'transaction': 'combined-run149', 'post_capture_number': 150,
        'status': 'fresh148-inputs-verified-awaiting-main-leaf-and-service-seal',
        'hardware_owner': 'main', 'hardware_operations_allowed': False,
        'arm_allowed': False, 'media_generation_allowed': False,
        'fresh': {'path': str(BASE / 'manifest.json'), 'sha256': FRESH, 'recovery_boot_id': BOOT},
        'native_actual': {'path': str(NATIVE), 'sha256': digest(path(NATIVE))},
        'actual_installed_versions': selected,
        'native_service_preservation': services,
        'previous_combined146_service': previous,
        'gpu_client_execute': False, 'gpu_client_count': 0,
        'gpu_client_log_baseline': str(LOGS / 'windows-rt-fresh148-baseline-client-files.json'),
        'gpu_baseline_count': 65, 'delayed_prior_logs_are_new147_execution': False,
        'install_targets': ['parent', 'thermal', 'msdc'],
        'msdc110': msdc,
        'parent1116': {'directory': str(parentdir), 'manifest_sha256': digest(path(parentdir / 'signed-manifest.json')),
                       'source_commit': parent['source_commit'], 'files': parent['files'],
                       'thermal_interface_version': 3, 'thermal_parent_build': 1104,
                       'headphone_interface_version': 2, 'headphone_parent_build': 1107},
        'thermal110': {'source_commit': leafsrc['source'], 'signed_package': None, 'main_build_owner': True},
        'bt': {'action': 'retain-existing-partial-root-no-first-install', 'root': bt,
               'child_present': False, 'radio_success': False},
        'retained_files': {n: f['sha256'] for n, f in files.items()
                           if n.startswith('Windows/System32/Drivers/') and Path(n).name not in changed},
        'planned_reboots': 1, 'boot_write_required': False, 'gpt_written': False,
        'benchmark_execute': False, 'wlan_scan_execute': False, 'hcd_new_candidate_adopted': False,
        'blockers': ['main署名中leaf1.10のexact manifest/member/seal結合',
                     '149専用service対象を定義しMSDCを追加（旧GPU位置依存を解消）、1TU増分compile/sign',
                     'GPUclient1/BT初回install/旧WiFi baselineの既存146専用処理を149専用policyへ分離',
                     'main独立seal・同Recovery無変更確認・media生成'],
        'pinned_files': [{'path': p, 'sha256': h} for p, h in sorted(pins.items())]
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = args.workspace.resolve(strict=True)
    out = args.out.resolve()
    require(out.is_relative_to(root) and not out.exists(), '新規root内出力のみ')
    result = prepare(root)
    out.mkdir(parents=True)
    (out / 'adoption-input.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'status': result['status'], 'pins': len(result['pinned_files']),
                      'GPUclient': 0, 'arm': False}, ensure_ascii=False))


if __name__ == '__main__':
    main()
