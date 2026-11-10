#!/usr/bin/env python3
"""fresh221/run222/post223 combined更新の入力準備。SCM/native/mediaは生成しない。"""
import argparse
import copy
import importlib.util
import hashlib
import json
from pathlib import Path, PureWindowsPath

sp = importlib.util.spec_from_file_location('client375', Path(__file__).with_name('prepare-gpu375-client-inputs.py'))
client = importlib.util.module_from_spec(sp)
sp.loader.exec_module(client)
a = client.m.admission
require = a.require
ACCEPTED = Path(__file__).with_name('gpu375-combined-accepted.json')
MODE = 'gpu376-battery18-wifi1031-client375'
PROFILE = 'combined-features-fresh221-gpu376-battery18-wifi1031-client375-v1'
SERVICE = DIRECTORY = client.SERVICE
shared = client.m.configure()
ROLES = shared.ROLES
TARGETS = shared.TARGETS
EXPECTED = {'display': '0.0.3.72', 'render': '0.0.3.76', 'battery': '0.0.1.8', 'wifi': '2026.11.11.1031'}
# Same order as the existing C install_order, filtered by explicit targets.
ORDER = ('display', 'render', 'wifi', 'battery')
BATTERY_READS = {
    r'SYSTEM\CurrentControlSet\Services\LC709203F\Parameters\NativeCheckpoint\LoadEpochV1': 16,
    r'SYSTEM\CurrentControlSet\Services\LC709203F\Parameters\NativeCheckpoint\FirstFailureV1': 96,
}


def installed_path(value):
    relative = PureWindowsPath(value)
    require(not relative.is_absolute() and not relative.drive and not relative.root and '..' not in relative.parts and relative.parts, '固定System32相対pathだけ')
    return 'Windows/System32/' + relative.as_posix()


def accepted():
    lock = a.read(ACCEPTED)
    require(lock['schema'] == 'GPU375_COMBINED_ACCEPTED_MANIFESTS_V1', '明示combined lock必須')
    require(lock.get('status') == 'actual-signed-manifests-reviewed', '実署名manifest/main review未確定。仮pin不可')
    require(all(isinstance(lock.get(k), dict) for k in ('gpu376', 'battery18', 'wifi1031', 'retained372')) and
            lock.get('display_update') is True, 'render376/transport372/UMD372 tuple確定が必要')
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
    if role == 'wifi':
        review = a.read(a.pinned(root, pin['main_review']))
        sys_sha = next(v['sha256'] for v in manifest['files'] if v['path'] == 'mt6630-combo-runtime.sys')
        require(manifest['signature_status'] == 'Valid' and manifest['signer_thumbprint'] == a.SIGNER and
                manifest['version'] == '2026.11.11.1031' and review['source_commit'] == pin['source_commit'] and
                review['manifest_sha256'] == ref['sha256'] and review['SYS_sha256'] == sys_sha and
                review['files_hash_size_verified'] is True and review['BCBF_Valid_and_CAT_membership_recorded'] is True and
                review['compiled_TUs'] == 19 and review['reused_objects'] == 24 and review['SDK_rebuild'] is False and
                review['hardware_success'] is False, '実1031署名/個別main reviewだけ。raw証拠を新boolで置換しない')
    if role == 'render':
        review = a.read(a.pinned(root, pin['main_review']))
        sys_sha = next(v['sha256'] for v in manifest['outputs'] if v['name'] == 'ge8300-render.sys')
        require(review['manifest_sha256'] == ref['sha256'] and review['signed_SYS_sha256'] == sys_sha and
                review['source'] == pin['runtime_source_commit'] and review['recipe'] == pin['source_commit'] and
                review['all_outputs_and_signature_logs_verified'] is True and review['all_other372_files_unchanged'] is True and
                review['BCBF_Valid_actual_file_bindings'] == 5 and review['hardware_verified'] is False, '実GPU376 main review tuple')
    if role == 'battery':
        review = a.read(a.pinned(root, pin['main_review']))
        require(manifest['signature_status'] == 'Valid' and manifest['certificate_thumbprint'] == a.SIGNER and
                manifest['catalog_membership_verified'] is True and manifest['hardware_tested'] is False and
                all(manifest[k] is False for k in ('charger_register_write', 'gauge_register_write', 'new_resource_owner')) and
                all(v == 0 for v in manifest['effective_flags'].values()), '実電池1.8署名/無HW書込みtuple')
        require(review['manifest_sha256'] == ref['sha256'] and review['source_commit'] == pin['source_commit'] and
                review['BCBF_Valid_and_CAT_membership_recorded'] is True and review['hardware_success'] is False,
                '実main電池review。実機成功へ昇格しない')
    # Existing role package validator checks manifest members, actual trio,
    # exact DriverVer/HWID/ClassGuid. A new native manifest source alias must
    # be implemented explicitly after its actual schema arrives, never guessed.
    return shared.base.package(root, row, role)


def prepare(root, spec, policy=None):
    lock = accepted()  # Reject before creating output or loading any private hive.
    require(spec.get('schema') == 'GPU375_COMBINED_PREPARE_INPUTS_V1' and spec.get('mode') == MODE,
            '既mask0と分離した明示combined modeのみ')
    require(spec.get('hardware_owner') == 'main' and spec.get('capture_number') == 221 and
            spec.get('run') == 222 and spec.get('post_capture') == 223, 'fresh221/run222/post223のみ')
    bundle = a.read(a.pinned(root, lock['main_review_bundle']))
    require(bundle['status'] == 'actual-signed-packages-main-reviewed' and bundle['hardware_verified'] is False and
            bundle['reviews'] == {k: lock[k]['main_review'] for k in ('gpu376', 'battery18', 'wifi1031')}, '固定main三package review結合')
    reviews = {k: lock[k]['main_review'] for k in ('gpu376', 'battery18', 'wifi1031')}
    for ref in reviews.values():
        a.pinned(root, ref)
    source_map = a.pinned(root, spec['client_map'])
    base_spec = a.read(source_map)
    require(base_spec.get('install_targets') == [] and base_spec.get('wifi_update') is None,
            '元375 mask0入力を変更せず参照')
    # Reuses actual221 inventory and all prior service/SYS/selection checks in
    # RAM. This is not RegLoadKey, a new inventory or a new executable profile.
    if policy is None:
        policy = client.load_map(source_map, root)
    targets = list(ORDER)
    require(set(spec['candidate_packages']) == set(targets), '更新対象の欠落/無関係role追加は禁止')
    pins = {'render': lock['gpu376'], 'display': lock['retained372'],
            'battery': lock['battery18'], 'wifi': lock['wifi1031']}
    rows = {r: checked_package(root, spec['candidate_packages'][r], r, pins[r]) for r in targets}
    baseline372 = a.read(a.pinned(root, lock['retained372']['manifest']))
    hashes372 = {v['name']: v['sha256'] for v in baseline372['outputs']}
    # Runtime UMD/FW originate in the actual372 package, not the old actual221
    # kernel tuple. Keep old captured hashes solely as pre-copy witnesses.
    extra_updates = {}
    for member in policy['packages']['render'].get('extra_members', []):
        new = next((x for x in rows['render'].get('extra_members', []) if x['name'] == member['name']), None)
        require(new is not None and installed_path(new['installed']) == installed_path(member['installed']) and new['sha256'] == hashes372[member['name']], 'UMD/FWは実372由来: ' + member['name'])
        a.pinned(root, {'path': (Path(lock['retained372']['manifest']['path']).parent / new['name']).as_posix(), 'sha256': new['sha256']})
        require(a.pinned(root, {'path': (Path(rows['render']['directory']) / new['name']).as_posix(), 'sha256': new['sha256']}).is_file(), '実UMD/FW leaf')
        installed = installed_path(new['installed'])
        extra_updates[installed] = dict(candidate=dict(path=(Path(rows['render']['directory']) / new['name']).as_posix(), sha256=new['sha256']),
                                        baseline=dict(path=policy['preserved_sources'][installed], sha256=policy['preserved_files'][installed]))
    for kind, name in rows['display']['names'].items():
        require(rows['display']['hashes'][kind] == hashes372['display-transport/' + name], 'transport372の実trio保持')
    require(spec.get('battery_checkpoint') is None, '電池exact2値はpost223同loadだけ。native221を再取得しない')
    # Baseline rows stay factual. A future policy consumer must remove updated
    # SYS from PreservedSYS and set mode=selected without replacing old hashes.
    return dict(schema='GPU375_COMBINED_PREPARED_INPUTS_V1', mode=MODE,
                status='inputs-verified-not-finalized', profile=client.PROFILE, service=client.SERVICE,
                capture_number=221, run=222, post_capture=223, client_map=spec['client_map'],
                accepted_manifest_lock_sha256=hashlib.sha256(ACCEPTED.read_bytes()).hexdigest(),
                main_package_reviews=reviews, candidate_packages=rows,
                updated_extra_members=extra_updates, retained372_manifest=lock['retained372']['manifest'],
                baselines={r: dict(copy.deepcopy(policy['baselines'][r]), mode='selected') for r in targets},
                install_targets=targets, DIF_order=targets,
                install_mask=sum(1 << ROLES.index(r) for r in targets),
                updated_SYS_excluded_from_preservation=['Windows/System32/Drivers/' + TARGETS[r][3] for r in targets],
                diagnostic_only=False, driver_updates=True, planned_reboots=1,
                boot_written=False, gpt_written=False, terminal_windows_hold=True,
                client_pair=base_spec['client_pair'], evaluator_source_commit=a.CLIENT375_SOURCE,
                post_capture_native_reader=dict(capture_number=223, additional_switches=['-IncludeBatteryFirstFailure'],
                    receipt_field='battery_first_failure', exact_path=r'ControlSet001\Services\LC709203F\Parameters\NativeCheckpoint',
                    exact_values={'LoadEpochV1': 16, 'FirstFailureV1': 96}, new_load_requested=False,
                    capture_manifest=None, native_receipt=None, observed=False),
                profile_finalized=False, SCM_generated=False, native_patch_performed=False,
                media_generation_allowed=False, device_operations=0, VM_operations=0)


def load_map(path, root):
    """Shared375 actual baseline, then only the admitted combined delta."""
    spec = a.read(path)
    require(spec.get('main_execution_approved') is True, 'mainの実承認入力必須。自動付与しない')
    p = client.load_map(a.pinned(root, spec['client_map']), root)
    delta = prepare(root, spec, p)
    p['profile'] = PROFILE
    p['capture_selection']['profile'] = PROFILE
    p.update(install_targets=list(ORDER), diagnostic_only=False, driver_updates=True, planned_reboots=1)
    p['observer_contract']['planned_reboots'] = 1
    p['display_transition'].update(DIF_order=list(ORDER), old_DOD_retirement='transport372/render376/WiFi1031/battery1.8 explicit update; boot98de retained')
    for role, row in delta['candidate_packages'].items():
        p['packages'][role] = copy.deepcopy(row)
        p['baselines'][role]['mode'] = 'selected'
        installed = 'Windows/System32/Drivers/' + TARGETS[role][3]
        p['preserved_files'].pop(installed, None)
        p['preserved_sources'].pop(installed, None)
    for row in p['packages']['render']['extra_members']:
        installed = installed_path(row['installed'])
        transition = delta['updated_extra_members'][installed]
        row['baseline_sha256'] = transition['baseline']['sha256']
        p['baseline_witnesses'][installed] = transition['baseline']
        p['preserved_files'].pop(installed, None)
        p['preserved_sources'].pop(installed, None)
    lock = accepted()
    p['baseline221_client_transition_reference_only'] = p.pop('gpu373_client_contract')
    for name in ('GPU371_manifest', 'GPU371_handoff'):
        p['baseline221_' + name + '_reference_only'] = p.pop(name)
    p['GPU376_manifest'] = lock['gpu376']['manifest']
    p['standard_present_contract'].update(driver_package_source_commit=p['packages']['render']['source_commit'],
                                        driver_signed_manifest_sha256=lock['gpu376']['manifest']['sha256'])
    pins = {v['path']: v['sha256'] for v in p['input_pins']}
    for row in delta['candidate_packages'].values():
        pins[row['manifest']] = row['manifest_sha256']
        for kind, name in row['names'].items():
            pins[(Path(row['directory']) / name).as_posix()] = row['hashes'][kind]
        for leaf in row.get('extra_members', []):
            pins[(Path(row['directory']) / leaf['name']).as_posix()] = leaf['sha256']
    # The validator reads the original372 UMD/FW as well as their identical
    # candidate376 copies. Both actual paths belong in the transfer closure.
    for leaf in p['packages']['render']['extra_members']:
        pins[(Path(lock['retained372']['manifest']['path']).parent / leaf['name']).as_posix()] = leaf['sha256']
    for ref in (spec['client_map'], lock['retained372']['manifest'], lock['main_review_bundle'], *delta['main_package_reviews'].values()):
        a.pinned(root, ref)
        pins[ref['path']] = ref['sha256']
    input_ref = dict(path=path.relative_to(root).as_posix(), sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    pins[input_ref['path']] = input_ref['sha256']
    p['input_pins'] = [dict(path=k, sha256=v) for k, v in sorted(pins.items())]
    p['gpu375_combined_contract'] = dict(schema='GPU375_COMBINED_TRANSITION_V1', mode=MODE,
        input=input_ref, lock_sha256=delta['accepted_manifest_lock_sha256'], run=222, fresh=221, post_capture=223,
        install_mask=0x528, post_capture_native_reader=delta['post_capture_native_reader'],
        battery_Code10_is_diagnostic_failure=True, readiness_gate_unchanged=True,
        hardware_success_claimed=False)
    return p


def validate_runtime_policy(root, policy):
    contract = policy['gpu375_combined_contract']
    require(contract['schema'] == 'GPU375_COMBINED_TRANSITION_V1' and contract['mode'] == MODE, '明示combined契約のみ')
    require(policy == load_map(a.pinned(root, contract['input']), root), '実入力からの全policy一致。旧371混在/ready緩和を拒否')


def generate(policy):
    # Emission has no capture/build side effect. Runtime admission independently
    # reloads actual pinned inputs; old mask0 validation is not loosened.
    require(policy['profile'] == PROFILE and policy['install_targets'] == list(ORDER) and
            policy['planned_reboots'] == 1 and policy['diagnostic_only'] is False, '固定combined modeだけ')
    lock = accepted()
    require(policy['GPU376_manifest'] == lock['gpu376']['manifest'], '正式376 pin必須')
    s = client.m.configure()
    s.PROFILE = s.base.PROFILE = PROFILE
    s.INSTALL = s.base.INSTALL = list(ORDER)
    return s.generate(policy)


paths = client.paths
assert_paths = client.assert_paths


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, required=True)
    parser.add_argument('--inputs', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--combined-update', action='store_true', required=True)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--prepare-only', action='store_true')
    mode.add_argument('--finalize', action='store_true')
    args = parser.parse_args()
    root = args.workspace.resolve(strict=True)
    input_path = client.m.rooted(root, args.inputs)
    result = prepare(root, a.read(input_path)) if args.prepare_only else load_map(input_path, root)
    header = None if args.prepare_only else generate(result)
    out = (root / args.out).resolve()
    require(out.is_relative_to(root) and not out.exists(), 'root内新規出力だけ')
    out.mkdir(parents=True)
    if args.prepare_only:
        (out / 'combined-candidate-inputs.json').write_text(json.dumps(result, indent=2) + '\n')
    else:
        (out / 'peripheral-batch-policy.h').write_text(header)
        (out / 'peripheral-service-path-policy.h').write_text(paths())
        for name in ('peripheral-policy.json', 'peripheral-preservation-plan.json'):
            (out / name).write_text(json.dumps(result, indent=2) + '\n')
    print('GPU375_COMBINED_INPUTS_PREPARED_NOT_FINALIZED' if args.prepare_only else 'GPU375_COMBINED_FINALIZED_NOT_BUILT_OR_DEPLOYED')


if __name__ == '__main__':
    main()
