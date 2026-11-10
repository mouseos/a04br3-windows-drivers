#!/usr/bin/env python3
"""fresh221/run222/post223 combined更新の入力準備。SCM/native/mediaは生成しない。"""
import argparse
import copy
import importlib.util
import hashlib
import json
from pathlib import Path

sp = importlib.util.spec_from_file_location('client375', Path(__file__).with_name('prepare-gpu375-client-inputs.py'))
client = importlib.util.module_from_spec(sp)
sp.loader.exec_module(client)
a = client.m.admission
require = a.require
ACCEPTED = Path(__file__).with_name('gpu375-combined-accepted.json')
MODE = 'gpu376-battery18-wifi1031-client375'
shared = client.m.configure()
ROLES = shared.ROLES
TARGETS = shared.TARGETS
EXPECTED = {'render': '0.0.3.76', 'battery': '0.0.1.8', 'wifi': '2026.11.11.1031'}
# Same order as the existing C install_order, filtered by explicit targets.
ORDER = ('display', 'render', 'wifi', 'battery')
BATTERY_READS = {
    r'SYSTEM\CurrentControlSet\Services\LC709203F\Parameters\NativeCheckpoint\LoadEpochV1': 16,
    r'SYSTEM\CurrentControlSet\Services\LC709203F\Parameters\NativeCheckpoint\FirstFailureV1': 96,
}


def accepted():
    lock = a.read(ACCEPTED)
    require(lock['schema'] == 'GPU375_COMBINED_ACCEPTED_MANIFESTS_V1', '明示combined lock必須')
    require(lock.get('status') == 'actual-signed-manifests-reviewed', '実署名manifest/main review未確定。仮pin不可')
    require(all(isinstance(lock.get(k), dict) for k in ('gpu376', 'battery18', 'wifi1031')) and
            type(lock.get('display_update')) is bool, '実tuple/transport保持または更新の確定が必要')
    return lock


def checked_package(root, row, role, pin):
    """Exact admitted native schema only; no flat manifest/provenance invention."""
    ref = {'path': row['manifest'], 'sha256': row['manifest_sha256']}
    require(ref == pin['manifest'], '承認済みexact manifest以外は拒否: ' + role)
    manifest = a.read(a.pinned(root, ref))
    require(manifest['schema'] == pin['schema'] and row['source_commit'] == pin['source_commit'], '実schema/sourceの一致: ' + role)
    require(manifest[pin['source_field']] == pin['source_commit'], '実manifest内source出自: ' + role)
    require(row['driver_version'] == pin['versions'][role] and row['driver_date'] == pin['dates'][role], '実INF tuple: ' + role)
    if role in EXPECTED:
        require(row['driver_version'] == EXPECTED[role], '既知376/1.8/1031だけ')
    # Existing role package validator checks manifest members, actual trio,
    # exact DriverVer/HWID/ClassGuid. A new native manifest source alias must
    # be implemented explicitly after its actual schema arrives, never guessed.
    return shared.base.package(root, row, role)


def prepare(root, spec):
    lock = accepted()  # Reject before creating output or loading any private hive.
    require(spec.get('schema') == 'GPU375_COMBINED_PREPARE_INPUTS_V1' and spec.get('mode') == MODE,
            '既mask0と分離した明示combined modeのみ')
    require(spec.get('hardware_owner') == 'main' and spec.get('capture_number') == 221 and
            spec.get('run') == 222 and spec.get('post_capture') == 223, 'fresh221/run222/post223のみ')
    require(spec.get('main_package_review') == lock['main_package_review'], '実main reviewのexact pin必須')
    a.pinned(root, spec['main_package_review'])
    source_map = a.pinned(root, spec['client_map'])
    base_spec = a.read(source_map)
    require(base_spec.get('install_targets') == [] and base_spec.get('wifi_update') is None,
            '元375 mask0入力を変更せず参照')
    # Reuses actual221 inventory and all prior service/SYS/selection checks in
    # RAM. This is not RegLoadKey, a new inventory or a new executable profile.
    policy = client.load_map(source_map, root)
    targets = [r for r in ORDER if r != 'display' or lock['display_update']]
    require(set(spec['candidate_packages']) == set(targets), '更新対象の欠落/無関係role追加は禁止')
    pins = {'render': lock['gpu376'], 'display': lock['gpu376'],
            'battery': lock['battery18'], 'wifi': lock['wifi1031']}
    rows = {r: checked_package(root, spec['candidate_packages'][r], r, pins[r]) for r in targets}
    # The signed kernel manifest must prove retained UMD/FW byte identity.
    for member in policy['packages']['render'].get('extra_members', []):
        new = next((x for x in rows['render'].get('extra_members', []) if x['name'] == member['name']), None)
        require(new is not None and new['installed'] == member['installed'] and new['sha256'] == member['sha256'], 'UMD/FW保持: ' + member['name'])
        require(a.pinned(root, {'path': str(Path(rows['render']['directory']) / new['name']), 'sha256': new['sha256']}).is_file(), '実UMD/FW leaf')
    if not lock['display_update']:
        gpu = a.read(a.pinned(root, lock['gpu376']['manifest']))
        outputs = {v['name']: v['sha256'] for v in gpu['outputs']}
        for kind, name in policy['packages']['display']['names'].items():
            require(outputs['display-transport/' + name] == policy['packages']['display']['hashes'][kind], 'transport369保持の実manifest証拠')
    # Optional separately captured two-value supplement: absence is recorded,
    # never turned into a success or silently merged into inventory221.
    battery = None
    if spec.get('battery_checkpoint') is not None:
        battery = a.read(a.pinned(root, spec['battery_checkpoint']))
        require(battery['source_manifest_sha256'] == policy['backup_manifest_sha256'] and
                battery['recovery_boot_id'] == policy['recovery_boot_id'] and
                battery['source_files'] == a.read(a.pinned(root, base_spec['native']))['source_files'], '電池補足は同221/SYSTEMだけ')
        require(battery['errors'] == [] and battery['originals_unchanged'] is True and
                battery['registry_write_requested'] is False, 'read-only実補足だけ')
        values = battery['captured_named_values']
        require(set(values) == set(BATTERY_READS), '電池補足はexact2値のみ')
        for key, size in BATTERY_READS.items():
            v = values[key]
            require(type(v['present']) is bool, '実present/absent必須')
            if v['present']:
                require(v['type'] == 3 and v['bytes'] == size and len(bytes.fromhex(v['raw_hex'])) == size, '固定REG_BINARY schema')
    # Baseline rows stay factual. A future policy consumer must remove updated
    # SYS from PreservedSYS and set mode=selected without replacing old hashes.
    return dict(schema='GPU375_COMBINED_PREPARED_INPUTS_V1', mode=MODE,
                status='inputs-verified-not-finalized', profile=client.PROFILE, service=client.SERVICE,
                capture_number=221, run=222, post_capture=223, client_map=spec['client_map'],
                accepted_manifest_lock_sha256=hashlib.sha256(ACCEPTED.read_bytes()).hexdigest(),
                main_package_review=spec['main_package_review'], candidate_packages=rows,
                baselines={r: dict(copy.deepcopy(policy['baselines'][r]), mode='selected') for r in targets},
                install_targets=targets, DIF_order=targets,
                install_mask=sum(1 << ROLES.index(r) for r in targets),
                updated_SYS_excluded_from_preservation=['Windows/System32/Drivers/' + TARGETS[r][3] for r in targets],
                diagnostic_only=False, driver_updates=True, planned_reboots=1,
                boot_written=False, gpt_written=False, terminal_windows_hold=True,
                client_pair=base_spec['client_pair'], evaluator_source_commit=a.CLIENT375_SOURCE,
                battery_checkpoint=spec.get('battery_checkpoint'), battery_checkpoint_observed=battery is not None,
                profile_finalized=False, SCM_generated=False, native_patch_performed=False,
                media_generation_allowed=False, device_operations=0, VM_operations=0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, required=True)
    parser.add_argument('--inputs', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--combined-update', action='store_true', required=True)
    parser.add_argument('--prepare-only', action='store_true', required=True)
    args = parser.parse_args()
    root = args.workspace.resolve(strict=True)
    input_path = client.m.rooted(root, args.inputs)
    result = prepare(root, a.read(input_path))
    out = (root / args.out).resolve()
    require(out.is_relative_to(root) and not out.exists(), 'root内新規出力だけ')
    out.mkdir(parents=True)
    (out / 'combined-candidate-inputs.json').write_text(json.dumps(result, indent=2) + '\n')
    print('GPU375_COMBINED_INPUTS_PREPARED_NOT_FINALIZED')


if __name__ == '__main__':
    main()
