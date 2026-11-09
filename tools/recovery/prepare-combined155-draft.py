#!/usr/bin/env python3
"""fresh154実capture待ちのmatched thermal1117模板。arm/媒体は生成しない。"""
import argparse
import hashlib
import json
from pathlib import Path

BASE = Path('port/xnu/out/windows-rt-thermal-acpi-consume1117')
PARENT = 'b2c1a3136a5b8eadf1900f71a8d330a6b0a31ac8'
FIRMWARE = 'b4ce1992af66adcefa02e34357d5dbf4313eaca2'
SYS = 'bbced952c499ba0588c77b861b20141043929773d703cdd327ffa92e61990979'
BOOT = '2e6cba6ab96cd5b586cefbd4bd004c6cee0484866ca6710ea454144da568c451'
STATUS = Path('port/xnu/out/mt8167-thermal-run145-rootfix/leaf110-main/client/mt8167-thermal-ioctl-status.exe')
STATUS_SHA = '542c7393ca79bba6fd244318747bb3fdf59d4469792566f4755085ba28c7cc8d'

def require(ok, why):
    if not ok:
        raise ValueError(why)

def read(p):
    return json.loads(p.read_text(encoding='utf-8-sig'))

def ref(root, relative, expected=None):
    p = (root / relative).resolve(strict=True)
    require(p.is_relative_to(root) and p.is_file(), 'Root-local existing file required')
    digest = hashlib.sha256(p.read_bytes()).hexdigest()
    require(expected is None or digest == expected, 'Input changed: ' + str(relative))
    return dict(path=str(relative), sha256=digest, bytes=p.stat().st_size)

def fixed_GPU_client(root):
    # Reuse the already sealed one-shot client contract; no driver adoption here.
    prior = read(root / 'port/xnu/out/windows-rt-combined146-peripheral/package-map.json')
    client = prior['diagnostics']['gpu_suite'].copy()
    require(client['sha256'] == '9934ba6bf9741630a544bf096ea3261c717d45873f91b899cc7e3eae62466add' and client['arguments'] == ['--hardware=PowerVR'] and client['timeout_ms'] == 900000 and client['phase'] == 1 and client['one_shot'], 'Exact9934 one-shot contract required')
    ref(root, Path(client['directory']) / client['exe'], client['sha256'])
    client['relative_directory'] = 'MT8167CombinedFollowup154\\clients'
    return client

def captured154(root):
    base = Path('port/xnu/out/private-windows-rt-prebackup-run154')
    manifest = ref(root, base / 'manifest.json', 'a26e82c937733617d932bb92201a593885575f27e89f26791612f791ca17b8da')
    m = read(root / manifest['path'])
    require(m['status'] == 'captured-unmounted' and m['read_only'] and m['unmounted'] and not m['errors'], 'Completed read-only154 required')
    require(m['recovery_boot_id'] == '9d795d5d-8d58-47a2-b208-e5c0149ecd19' and len(m['files']) == 41, 'Exact actual154 required')
    files = []
    for row in m['files']:
        f = ref(root, base / row['backup_path'], row['sha256'])
        require(f['bytes'] == row['bytes'], '154 size mismatch')
        files.append(dict(source_path=row['source_path'], **f))
    gpu = [f for f in files if f['source_path'] == 'Windows/System32/Drivers/ge8300-render.sys']
    require(len(gpu) == 1 and gpu[0]['sha256'] == '431c06ade452b0ca00a920419bb39fe5261080469750a0bdf9f61670cccbec52', 'Actual342 required')
    native = Path('port/xnu/out/private-windows-rt-combined-run153-registry-post')
    receipts = [ref(root, native / n) for n in ('receipt.json', 'init-receipt.json')]
    for r in receipts:
        v = read(root / r['path'])
        require(v['source_manifest_sha256'] == manifest['sha256'] and not v['errors'] and v['load_rc'] == v['unload_rc'] == v['privilege_restore_error'] == 0 and v['originals_unchanged'], 'Clean154 native receipt required')
        require(v['source_files'] == {f['source_path'].split('/')[-1]: f['sha256'] for f in files if f['source_path'] in ['Windows/System32/config/' + n for n in ('SYSTEM', 'SYSTEM.LOG1', 'SYSTEM.LOG2')]}, 'Receipt must bind same captured SYSTEM trio')
        require(v['module_identity']['expected_identity_matches'] and v['module_identity']['observed_buildid'] == 'GPU342-tq-clear-v1', 'Same capture342 module identity required')
    init = read(root / receipts[1]['path'])['gpu_init']
    require(init['ReachedMask']['value'] == 7 and init['DriverEntry']['value'] == init['Contract']['value'] == 0 and (init['DxgkInitialize']['value'] & 0xffffffff) == 0xc000003b, 'Actual154 initialization boundary changed')
    return {'required_capture_number': 154, 'fresh': manifest, 'recovery_boot_id': m['recovery_boot_id'],
      'same_recovery_unchanged_proof': None, 'native_actual': receipts, 'actual_selected_versions': None,
      'original_files': files, 'mutable_log_baseline': None,
      'ModuleIdentity_same_capture': read(root / receipts[0]['path'])['module_identity'],
      'gpu_init_same_capture': init, 'later_unreached_stage_values_are_current_success': False,
      'result153': {'diagnostic_terminal': 21, 'finalization': 0, 'GPU_client_executed': False}}

def make_draft(root):
    seal_ref = ref(root, BASE / 'main-independent-seal.json')
    seal = read(root / seal_ref['path'])
    require(seal['status'] == 'main-independent-matched-thermal1117-artifact-seal-not-device-tested', 'Exact main seal required')
    require(seal['parent_source_commit'] == PARENT and seal['firmware_source_commit'] == FIRMWARE, 'Matched source required')
    require(seal['parent_SYS_sha256'] == SYS and seal['boot_sha256'] == BOOT, 'Matched binaries required')
    require(seal['mixed_old_new_allowed'] is False and seal['EDK_common_changed'] is False, 'Closed matched contract required')
    pins_ref = ref(root, BASE / 'pins.json', seal['pins_sha256'])
    members = [ref(root, e['path'], e['sha256']) for e in read(root / pins_ref['path'])]
    sys_ref = ref(root, BASE / 'package/parent/mt8167-shared.sys', SYS)
    boot_ref = ref(root, BASE / 'firmware/boot-final/boot.img', BOOT)
    prior = ref(root, Path('port/xnu/out/windows-rt-combined153-fresh152/main-deployment-contract.json'))
    require(read(root / prior['path'])['transaction'] == 'combined-run153', 'Frozen153 reference required')
    return {
      'schema': 1, 'profile': 'combined-features-fresh154-DRAFT', 'transaction': 'combined-run155',
      'status': 'fresh154-bound-matched1117-GPU343-awaiting-main-seal', 'draft': True,
      'arm_allowed': False, 'normal_install_allowed': False, 'media_generation_allowed': False,
      'hardware_owner': 'main', 'hardware_operations_allowed': False,
      'source_basis': 'beb3edccb125ebb38cb001c23782487c67dbebb9',
      'frozen153_reference_only': prior,
      'capture_selection': captured154(root),
      'run153_reported_result': {
        'provenance': 'main report plus captured154 receipt/init-receipt bound below',
        'diagnostic_terminal': 21, 'finalization': 0, 'planned_restart_count': 1,
        'GPU_node': {'target_index': 10, 'problem': 37, 'PB204': '0x25'},
        'GPU_client_start_PB220_present': False, 'GPU_client_executed': False,
        'Clear_executed': False, 'GPU_rendering_success': False,
        'ModuleIdentity': 'capture_selection.ModuleIdentity_same_capture', 'InitCheckpoint': 'capture_selection.gpu_init_same_capture',
        'link_contract_comparison_reported': {
          'old341': {'COFF': '0x122', 'LAA': True, 'DLL': '0x140'},
          'new342': {'COFF': '0x102', 'LAA': False, 'DLL': '0x8140'},
          'root_cause_confirmed': False, 'investigation_owner': 'main'}},
      'GPU343_adoption': {
        'scope_requested_by_main': True, 'selected_for_deployment': False,
        'artifact': None, 'main_seal': None,
        'source_branch': 'work/ge8300-msvc-relink343', 'recipe_source_reported': '323b3207',
        'compiled_source': '59ded879bab1c4c54abac73bd2b673adada33d4c',
        'runtime_buildid': 'GPU342-tq-clear-v1', 'INF_version': '0.0.3.43',
        'expected_sourcebase': 'eb2bc820c3b67168b16f50dbe3fbf27f513ddfae',
        'same80objects_relink_compile_count_reported': 0,
        'root_cause_uniquely_proven': False, 'main_seal_required_before_arm': True,
        'ModuleIdentity_distinguishes342_from343': False,
        'physical_SYS_hash_and_selected_INF_required_to_distinguish_relink': True},
      'matched_candidate': {'main_seal': seal_ref, 'pins': pins_ref, 'verified_members': members,
        'parent_source_commit': PARENT, 'firmware_source_commit': FIRMWARE,
        'parent': sys_ref, 'boot': boot_ref, 'boot_write_required': True,
        'mixed_old_new_allowed': False, 'ATCV_required': 1, 'ATCO_required': 1,
        'ATCO_one_shot_per_boot': True, 'Windows_ACPI_ioctl_verified': False,
        'hardware_temperature_verified': False, 'existing_signed_artifacts_rebuild': False},
      'service_template': {'service': 'MT8167CombinedFollowup154', 'data_directory': 'MT8167CombinedFollowup154',
        'targets': 12, 'install_targets': ['parent', 'render'], 'parent_target_index': 6,
        'install_mask': 1088, 'first_install_mask': 0, 'planned_reboots': 1,
        'signed_service': None, 'native_arm': None, 'policy_hash': None,
        'compile_changed_service_TUs_after_adoption': 1,
        'keep_existing_SystemRequired_bounded_request': True},
      'execution_template': {
        'temperature': {'mechanism': 'builtin', 'ioctl': '0x226810', 'output_bytes': 28,
          'count': 1, 'exact_NTSTATUS_required_on_failure': True},
        'status': {**ref(root, STATUS, STATUS_SHA), 'arguments': ['--status'],
          'ioctl': '0x226814', 'output_bytes': 64, 'count': 1, 'timeout_ms': 10000,
          'phase': 0, 'after': 'temperature-completion-including-failure',
          'additional_measurement': False, 'stdout_log': 'logs/thermal_status.stdout.txt',
          'binary_cwd_relative': 'thermal-ioctl-status-v1.bin',
          'exit0_does_not_prove_temperature_success': True},
        'GPU_client_count': 1, 'GPU_client_must_be_last': True,
        'GPU_client': fixed_GPU_client(root), 'audio_client_count': 0, 'WLAN_query_count': 0,
        'BT_query_count': 0, 'BT_first_install_count': 0, 'benchmark_count': 0,
        'clock_client_count': 0, 'LC_client_count': 0, 'ACCDET_client_count': 0,
        'rotation_client_count': 0, 'AFE_client_count': 0,
        'observer': {'install_timeout_seconds': 900, 'boot_wait_seconds': 180,
          'diagnostic_seconds_from_first_phase1_PB286': 180,
          'deadline_update_max': 1, 'GPU_extension_seconds': 900, 'GPU_deadline_start': 'first current PB target0/event0x220; once',
          'postreboot_diagnostic_total_timeout_seconds': 1080,
          'UART_normal_boot_and_Recovery_verification_owner': 'main'}},
      'runtime_header_template': {'A04_GPU_PHASE1_ENABLED': 1, 'A04_RUN_THERMAL_READ': 1,
        'A04_RUN_WLAN_READ': 0, 'A04_RUN_BT_READ': 0,
        'A04_PERIPHERAL_INSTALL_MASK': 1088, 'A04_PERIPHERAL_FIRST_INSTALL_MASK': 0,
        'A04_DIAGNOSTIC_CLIENT_COUNT': 2, 'not_a_compilable_policy_until_actual154_bound': True},
      'preservation_template': {'leaf_version': '1.10', 'leaf_actual154_hash': None,
        'all_other_actual154_driver_tuples': None, 'service_fingerprints': None,
        'named_values': None, 'Service152_stopped_values': None,
        'missing_hashes_must_not_be_synthesized': True, 'BT_existing_ROOT_retained': True,
        'GPT_written': False, 'common_EDK_changed': False,
        'immutable_hive_driver_gate_retained': True,
        'retained_GPU_problem_is_observation_not_driver_byte_mismatch': True,
        'GPU_started_success_must_not_be_inferred_from_retention': True,
        'mutable_logs': 'archive/prefix observation separately; never immutable install admission'},
      'post_capture_number': 156,
      'adoption_requirements': [
        'Bind real fresh154 manifest/original41/native actual and same Recovery proof; never relabel152',
        'Join153 actual results; one343 GPU client only; no unchanged audio/wireless/benchmark diagnostics',
        'Derive all retained tuples and latest stopped service values including Service152 from native actual154',
        'Generate155 validator/native reader/service policy/header with one final identical policy hash',
        'Apply exact parent1117 plus exact boot2e6 in one update; reject incomplete/mixed package before writes',
        'Reuse signed parent/FW; service1TU only after formal adoption; independent main seal before deployment',
        'Collect temperature28B then status64B once in same boot even if temperature failed; preserve exact status',
        'main verifies UART boot/restart/recovery; artifact/AML evidence is not Windows temperature success'],
      'limitations': ['No native hive, service binary or deployable media generated',
        'fresh154 captured originals/native verified; main343 seal and same-Recovery proof still pending; no arm allowed']}

def reject_arm(draft):
    require(draft['draft'] and not any(draft[k] for k in
        ('arm_allowed', 'normal_install_allowed', 'media_generation_allowed')), 'Draft flags altered')
    raise ValueError('343 main seal/正式policy未結合:155 draftはarm・媒体生成不可')

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workspace', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    root = a.workspace.resolve(strict=True)
    out = a.out.resolve()
    require(out.is_relative_to(root) and not out.exists(), 'New root-local output required')
    draft = make_draft(root)
    out.mkdir(parents=True)
    (out / 'contract-DRAFT.json').write_text(json.dumps(draft, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'status': draft['status'], 'verified_members': len(draft['matched_candidate']['verified_members']),
                      'arm': False, 'media': False}, ensure_ascii=False))

if __name__ == '__main__':
    main()
