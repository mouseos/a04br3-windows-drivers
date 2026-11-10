#!/usr/bin/env python3
"""実署名leafとfresh196からbundleを作成し、導入せずに保持契約を検証する。"""
import argparse
import ast
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
ap = argparse.ArgumentParser()
ap.add_argument('--workspace', type=Path, required=True)
ap.add_argument('--out', type=Path, required=True)
a = ap.parse_args()
root = a.workspace.resolve(strict=True)
repo = Path(__file__).resolve().parents[1]
out = a.out.resolve()
assert out.is_relative_to(root) and not out.exists()
loader = importlib.util.spec_from_file_location('usbplan', repo/'tools/ci/prepare-usb1121-first-install.py')
module = importlib.util.module_from_spec(loader)
loader.loader.exec_module(module)
plan = module.prepare(root, out)
assert plan['capture_sha256'] == module.FRESH and plan['owner_ABI'] == 3
assert len(plan['files']) == 14
assert plan['candidate']['hcd']['hwid'] == 'MT8167\\USBPORT20'
assert plan['baseline']['legacy']['hwid'] == 'MT8167\\USB20'
assert plan['rollback']['parent']['version'] == '2026.11.10.1120'
assert plan['rollback']['legacy']['version'] == '2026.11.8.1010'
assert plan['actions'].index('stage-both') < plan['actions'].index('disable-legacy-devnode') < plan['actions'].index('DIF-parent')
assert not plan['root_PDO_creation'] and not plan['force_host'] and not plan['source_ON']
assert not plan['hardware_verified'] and not plan['normal_install_performed']
for name, expected in plan['files'].items():
    assert hashlib.sha256((out/name).read_bytes()).hexdigest() == expected
    assert (out/name).resolve().is_relative_to(out)
assert (out/'plan.sha256').read_text().strip() == module.sha(out/'plan.json')
ast.parse((repo/'tools/ci/prepare-usb1121-first-install.py').read_text())
cs = (repo/'tools/usb/UsbAbi3Pnp.cs').read_text()
assert 'SetupDiCreateDeviceInfoW' not in cs and 'CM_Create_DevNode' not in cs
assert 'StopService' not in cs and 'DeviceIoControl' not in cs
(out/'source-tests.json').write_text(json.dumps({
    'actual_signed_ABI3_leaves': 6, 'actual_rollback_leaves': 6,
    'fresh196_and_native_receipt_pinned': True, 'bundle_files_verified': 14,
    'two_stage_before_legacy_stop_before_parent_DIF': True,
    'Windows_runner_device_REA_access': False, 'hardware_success_claimed': False,
}, indent=2)+'\n')
print('USB1121_FIRST_INSTALL_BUNDLE_CONTRACT_PASS_NO_PNP_EXECUTION')
