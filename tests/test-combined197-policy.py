#!/usr/bin/env python3
"""保存済みfresh196を検証し、未署名367のfinalize拒否と既存生成経路を確認する。"""
import argparse
import ast
import copy
import importlib.util
import json
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
ap = argparse.ArgumentParser()
ap.add_argument('--workspace', type=Path, required=True)
ap.add_argument('--map', type=Path, required=True)
ap.add_argument('--out', type=Path, required=True)
a = ap.parse_args()
root = a.workspace.resolve(strict=True)
repo = Path(__file__).resolve().parents[1]
out = a.out.resolve()
assert out.is_relative_to(root) and not out.exists()
spec = json.loads((root / a.map).read_text())
file = repo / 'tools/recovery/prepare-peripheral-followup196.py'
loader = importlib.util.spec_from_file_location('run197', file)
module = importlib.util.module_from_spec(loader)
loader.loader.exec_module(module)
pins, pin, fresh, originals, native, prior, values, previous, baselines = module.audit_baseline(root, spec)
assert len(originals) == 42 and len(values) == 125 and len(native['old_services']) == 18
assert native['post_ownership_policy360']['raw'] == '00000000'

def rejected(candidate, expected):
    try:
        module.build_policy(root, candidate)
    except (ValueError, AssertionError) as error:
        assert expected in str(error), str(error)
    else:
        raise AssertionError('拒否すべきprofileが通過した')

rejected(spec, 'Pending signed367 input')
wrong = copy.deepcopy(spec)
wrong['install_targets'] = ['parent', 'usb', 'render']
rejected(wrong, 'Exact run197 scope')
wrong = copy.deepcopy(spec)
wrong['fresh']['sha256'] = '0' * 64
rejected(wrong, 'Input hash')
# 実在する旧署名leafでもGPU367として受け入れない。
wrong = copy.deepcopy(spec)
oldmap = json.loads((root / 'port/xnu/out/windows-rt-combined195-fresh194/package-map.json').read_text())
wrong['GPU367_manifest'] = oldmap['GPU366_manifest']
wrong['GPU367_handoff'] = oldmap['GPU366_handoff']
rejected(wrong, 'Exact GPU367 candidate')

# 既存tupleを使う生成処理だけの確認。final policy/mediaを作成しない。
fixture = copy.deepcopy(prior)
fixture.update(profile=module.PROFILE, service=module.SERVICE, data_directory=module.SERVICE,
               captured_registry_values=values, normal_install_allowed=False)
header = module.generate(fixture)
assert '#define A04_PERIPHERAL_INSTALL_MASK 0x400u' in header
assert '#define A04_DIAGNOSTIC_CLIENT_COUNT 1u' in header
assert '#define A04_PERIPHERAL_FIRST_INSTALL_MASK 0u' in header
assert '#define A04_PERIPHERAL_DEFERRED_UNBOUND_MASK 0u' in header
assert 'MT8167CombinedFollowup196' in module.paths()
for role, package in prior['packages'].items():
    assert package['names']['sys'] in header

changed_python = [file, repo/'tools/winpe/combined196_policy.py',
                  repo/'tools/winpe/private_hardware_probe_input.py',
                  repo/'tools/winpe/build-private-peripheral-batch-media.py']
for path in changed_python:
    ast.parse(path.read_text(), filename=str(path))
subprocess.run(['bash', '-n', str(repo/'tools/recovery/collect-combined198.sh')], check=True)
out.mkdir(parents=True)
(out/'result.json').write_text(json.dumps({
    'baseline_files_verified': 42, 'old_full_services': 18, 'named_values': 125,
    'POST_retained': '00000000', 'pending_and_old_candidate_rejected': True,
    'wrong_capture_and_USB_targets_rejected': True, 'generation_fixture_only': True,
    'python_AST_and_collector_shell_syntax': True,
    'final_policy_created': False, 'Windows_runner_device_REA_access': False,
}, indent=2) + '\n')
print('FRESH196_AUDIT_AND_FAIL_CLOSED_GENERATION_PASS_NOT_FINALIZED')
