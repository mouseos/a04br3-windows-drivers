#!/usr/bin/env python3
"""fresh164未到着時の165非arm模板。媒体・service・registryは生成しない。"""
import argparse
import hashlib
import json
from pathlib import Path

PRIOR = 'port/xnu/out/windows-rt-combined163-fresh162/main-deployment-contract.json'
PRIOR_SHA = '8663aa2f9389dfbc9965b28c7ccec0404084963edd0b298bc3c2b6790817ec90'
BT = 'port/xnu/out/mt6630-broker-start2016/'
BT_PINS_SHA = 'e4147809e033155ef38a33283337e4fd1a5a8da210c7f089d67c7047556cdaff'

def require(ok, message):
    if not ok:
        raise ValueError(message)

def read(p):
    return json.loads(p.read_text(encoding='utf-8-sig'))

def ref(root, name, expected=None):
    p = (root / name).resolve(strict=True)
    require(p.is_relative_to(root) and p.is_file(), 'Root-local input required')
    h = hashlib.sha256(p.read_bytes()).hexdigest()
    require(expected is None or h == expected, 'Pinned input changed: ' + name)
    return {'path': p.relative_to(root).as_posix(), 'sha256': h, 'bytes': p.stat().st_size}

def make(root):
    prior = ref(root, PRIOR, PRIOR_SHA)
    c = read(root / prior['path'])
    p = read(root / ref(root, c['policy_file'], c['policy_file_sha256'])['path'])
    require(c['transaction'] == 'combined-run163' and c['install_targets'] == ['render'], 'Frozen163 reference required')
    require(len(p['captured_registry_values']) == 40 and len(p['service_preservation']) == 18, 'Prior40/18 reference required')
    bp = ref(root, BT + 'pins.json', BT_PINS_SHA)
    b = read(root / bp['path'])
    require(b['source_commit'] == '6ca61e186eb2c235bcc3a21579eb8cbae8fd9ede', 'Exact BT2016 source required')
    for f in b['files']:
        ref(root, f['path'], f['sha256'])
    batch = read(root / (BT + 'batch-input.json'))
    require(batch['versions'] == {'client': 2016, 'provider': 1025} and batch['requires_both'], 'Paired admission ABI required')
    return {
        'schema': 1, 'transaction': 'combined-run165', 'status': 'DRAFT-await-real-fresh164',
        'draft': True, 'arm_allowed': False, 'normal_install_allowed': False,
        'media_generation_allowed': False, 'hardware_owner': 'main', 'hardware_operations_allowed': False,
        'prior163_reference_only': prior,
        'capture_selection': {
            'capture_number': 164, 'fresh_source': None, 'fresh_manifest': None,
            'recovery_boot_id': None, 'same_recovery_unchanged_proof': None,
            'original41': None, 'native_post_receipt': None,
            'actual_driver_selection': None, 'actual_GPU_result': None,
            'actual_GPU_SYS_INF_BuildID': None, 'mutable_log_baseline': None,
        },
        'BT_candidate': {'pins': bp, 'batch_input': ref(root, BT + 'batch-input.json'),
            'source_commit': b['source_commit'], 'client_version': 2016, 'provider_version': 1025,
            'both_required': True, 'admission_ioctl': '0x94a', 'admission_bytes': 32,
            'hardware_verified': False, 'Code10_resolved': False,
            'D0Return0_is_HCI_success': False, 'child_published_only_after_real_HCI_OPEN': True,
            'provider_wait_bound_seconds': 30, 'provider_wait_attempt_bound': 300},
        'service_template': {'service': 'MT8167CombinedFollowup164', 'targets': 13,
            'install_targets': ['wifi', 'bt'], 'install_mask': (1 << 3) | (1 << 12),
            'first_install_mask': 0, 'planned_restarts': 1,
            'root_registration_count': 0, 'signed_service': None, 'native_proof': None,
            'incremental_service_TUs_after_finalization': 1},
        'execution_template': {'diagnostic_client_count': 1, 'BT_status_count': 1,
            'BT_status_index': 0, 'BT_status_arguments': ['--status'],
            'BT_status_timeout_ms': 30000, 'BT_status_client_exact': None,
            'GPU_client_count': 0, 'GPU_client_index': None,
            'temperature_count': 0, 'thermal_status_count': 0, 'audio_count': 0,
            'WLAN_count': 0, 'benchmark_count': 0, 'other_clients': 0,
            'install_timeout_seconds': 900,
            'deadline': {'boot_wait_seconds_from_resume': 180,
                'service_budget_seconds': 180, 'service_start_event': 'phase1 first PB286',
                'deadline_updates_per_phase_max': 1, 'repeated_events_extend': False,
                'same_boot_required': True, 'max_total_seconds_after_resume': 360}},
        'preservation_template': {'boot_write_required': False,
            'expected_retained_boot_sha256': c['matched_boot_sha256'],
            'parent1117_retained': True, 'thermal1_10_retained': True,
            'actual164_GPU_retained': None, 'actual164_other_tuples': None,
            'old40_named_reference_only': p['captured_registry_values'],
            'service162_actual5': None, 'fresh164_full_service_fingerprints': None,
            'expected_named_count_after_actual5_verified': 45,
            'final45_named_values': None, 'Start4_not_assumed': True},
        'GPU348': {'selected': False, 'source': None, 'signed_artifact': None, 'main_seal': None,
            'requires_explicit_main_scope_update': True},
        'reader_template': {'post_capture_number': 166,
            'base_source': 'tools/recovery/read-combined162-post.ps1',
            'BT_helper_source': 'tools/recovery/read-mt6630-start-checkpoint.ps1',
            'BT_helper_commit': b['source_commit'],
            'same_clone_mount_OpenSubKey_readonly': True,
            'live_root_forbidden': True, 'GPU_expected_buildid': None,
            'same_boot_PID_checksum_oldappend_separate': True,
            'archive_failure_evidence_before_receipt_error_check': True},
        'final_artifacts': {'policy': None, 'service': None, 'native': None, 'media': None, 'contract': None},
        'required_adoption_checks': [
            'main supplies verified real fresh164/Recovery/original41/postreceipt/actualGPU; never relabel162',
            'append five raw typed service162 values from actual hive; prove stopped and preserve old40',
            'derive45named/fullservice/all retained tuples from actual164 and verify load-unload-restore',
            'separate immutable hives-drivers from mutable logs; archive and prefix-compare actual164 logs',
            'use existing BT status signed client exact pins; new registration0; one status invocation only',
            'default GPU retain and client0; GPU348 requires main explicit scope decision and completed seal',
            'formal source-policy-validator-reader-service1TU-native-media contract must share final identity',
            'main independent seal and same-Recovery recheck before arm'],
    }

def reject_media(d):
    require(d.get('transaction') == 'combined-run165', 'Wrong transaction')
    raise ValueError('DRAFT165_NO_ARM: fresh164/source/Recovery/service162 raw5/45named/actualGPU未結合。正式化前の媒体生成禁止')

def main():
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('--workspace', type=Path, required=True)
    g = a.add_mutually_exclusive_group(required=True)
    g.add_argument('--out', type=Path)
    g.add_argument('--check-media', type=Path)
    v = a.parse_args(); root = v.workspace.resolve(strict=True)
    try:
        if v.check_media:
            q = v.check_media.resolve(strict=True); require(q.is_relative_to(root), 'Root-local draft required')
            reject_media(read(q))
        d = make(root); out = v.out.resolve()
        require(out.is_relative_to(root) and not out.exists(), 'New root-local output required')
        out.mkdir(parents=True)
        (out / 'contract-DRAFT.json').write_text(json.dumps(d, ensure_ascii=False, indent=2) + '\n')
        print('RUN165_DRAFT_NO_ARM_NO_MEDIA_AWAIT_REAL164')
    except ValueError as e:
        a.exit(2, str(e) + '\n')

if __name__ == '__main__':
    main()
