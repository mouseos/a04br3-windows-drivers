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
      'status': 'matched1117-sealed-awaiting-actual-fresh154', 'draft': True,
      'arm_allowed': False, 'normal_install_allowed': False, 'media_generation_allowed': False,
      'hardware_owner': 'main', 'hardware_operations_allowed': False,
      'source_basis': 'beb3edccb125ebb38cb001c23782487c67dbebb9',
      'frozen153_reference_only': prior,
      'capture_selection': {'required_capture_number': 154, 'fresh': None, 'recovery_boot_id': None,
        'same_recovery_unchanged_proof': None, 'native_actual': None, 'actual_selected_versions': None,
        'original_files': None, 'mutable_log_baseline': None, 'result153': None},
      'run153_reported_result': {
        'provenance': 'main report; actual154 files still pending',
        'diagnostic_terminal': 21, 'finalization': 0, 'planned_restart_count': 1,
        'GPU_node': {'target_index': 10, 'problem': 37, 'PB204': '0x25'},
        'GPU_client_start_PB220_present': False, 'GPU_client_executed': False,
        'Clear_executed': False, 'GPU_rendering_success': False,
        'ModuleIdentity': None, 'InitCheckpoint': None,
        'link_contract_comparison_reported': {
          'old341': {'COFF': '0x122', 'LAA': True, 'DLL': '0x140'},
          'new342': {'COFF': '0x102', 'LAA': False, 'DLL': '0x8140'},
          'root_cause_confirmed': False, 'investigation_owner': 'main'}},
      'optional_GPU343_adoption': {
        'selected': False, 'artifact': None, 'main_seal': None,
        'default_scope_remains_parent_only': True,
        'actual154_GPU342_Code37_must_not_be_relabelled_as_started': True,
        'requirements': [
          'main completes evidence-backed link-contract fix and independently seals exact343 tuple',
          'main explicitly selects combined parent1117+boot2e6+GPU343 scope',
          'Bind real154 actual342 Code37 and actual ModuleIdentity/InitCheckpoint as baseline',
          'Reconcile install mask/order, expected post identity, reader, clients/timeouts and final policy hash together',
          'Do not arm343 using this parent-only draft or claim153 ran Clear'],
        'GPU_client_execution_authorized_by_this_draft': False},
      'matched_candidate': {'main_seal': seal_ref, 'pins': pins_ref, 'verified_members': members,
        'parent_source_commit': PARENT, 'firmware_source_commit': FIRMWARE,
        'parent': sys_ref, 'boot': boot_ref, 'boot_write_required': True,
        'mixed_old_new_allowed': False, 'ATCV_required': 1, 'ATCO_required': 1,
        'ATCO_one_shot_per_boot': True, 'Windows_ACPI_ioctl_verified': False,
        'hardware_temperature_verified': False, 'existing_signed_artifacts_rebuild': False},
      'service_template': {'service': 'MT8167CombinedFollowup154', 'data_directory': 'MT8167CombinedFollowup154',
        'targets': 12, 'install_targets': ['parent'], 'parent_target_index': 6,
        'install_mask': 64, 'first_install_mask': 0, 'planned_reboots': 1,
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
        'GPU_client_count': 0, 'audio_client_count': 0, 'WLAN_query_count': 0,
        'BT_query_count': 0, 'BT_first_install_count': 0, 'benchmark_count': 0,
        'clock_client_count': 0, 'LC_client_count': 0, 'ACCDET_client_count': 0,
        'rotation_client_count': 0, 'AFE_client_count': 0,
        'observer': {'install_timeout_seconds': 900, 'boot_wait_seconds': 180,
          'diagnostic_seconds_from_first_phase1_PB286': 180,
          'deadline_update_max': 1, 'GPU_extension_seconds': 0,
          'UART_normal_boot_and_Recovery_verification_owner': 'main'}},
      'runtime_header_template': {'A04_GPU_PHASE1_ENABLED': 0, 'A04_RUN_THERMAL_READ': 1,
        'A04_RUN_WLAN_READ': 0, 'A04_RUN_BT_READ': 0,
        'A04_PERIPHERAL_INSTALL_MASK': 64, 'A04_PERIPHERAL_FIRST_INSTALL_MASK': 0,
        'A04_DIAGNOSTIC_CLIENT_COUNT': 1, 'not_a_compilable_policy_until_actual154_bound': True},
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
        'Join153 actual results; no repeat of unchanged GPU/audio/wireless/benchmark diagnostics',
        'Derive all retained tuples and latest stopped service values including Service152 from native actual154',
        'Generate155 validator/native reader/service policy/header with one final identical policy hash',
        'Apply exact parent1117 plus exact boot2e6 in one update; reject incomplete/mixed package before writes',
        'Reuse signed parent/FW; service1TU only after formal adoption; independent main seal before deployment',
        'Collect temperature28B then status64B once in same boot even if temperature failed; preserve exact status',
        'main verifies UART boot/restart/recovery; artifact/AML evidence is not Windows temperature success'],
      'limitations': ['No native hive, service binary or deployable media generated',
        'fresh154 actual and Recovery identity intentionally unset; no arm allowed']}

def reject_arm(draft):
    require(draft['draft'] and not any(draft[k] for k in
        ('arm_allowed', 'normal_install_allowed', 'media_generation_allowed')), 'Draft flags altered')
    raise ValueError('fresh154実capture/native actual未結合:155 draftはarm・媒体生成不可')

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
