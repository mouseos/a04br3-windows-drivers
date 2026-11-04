#!/usr/bin/env python3
"""Create a WinPE copy using one intact, already-built driver package."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--wimlib', default='wimlib-imagex')
p.add_argument('--source-wim', type=Path, required=True)
p.add_argument('--package', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--startnet', type=Path, help='Override the default diagnostic startnet script')
p.add_argument('--guarded-test', type=Path, help='Optional ARM32 guarded-write diagnostic executable')
p.add_argument('--bcd-export', type=Path, help='Optional ARM32 guarded BCD export executable')
p.add_argument('--button-inject', type=Path, help='Optional ARM32 button notification diagnostic')
p.add_argument('--button-driver', type=Path, help='Optional directory with original RT msgpiowin32 INF and SYS')
p.add_argument('--root-repair', type=Path, help='Optional guarded NTFS root-attribute repair executable')
p.add_argument('--volume-guard', type=Path, help='Optional read-only Windows-volume identity guard')
p.add_argument('--button-probe-service', type=Path, help='Optional one-shot Windows interface probe service')
p.add_argument('--button-probe-original', type=Path, help='Expected existing diagnostic executable for guarded replacement')
p.add_argument('--compare-files', type=Path, help='Self-contained file comparison executable')
p.add_argument('--button-kernel-probe', type=Path, help='Optional one-shot kernel interface probe')
p.add_argument('--flush-probe-reference', type=Path, help='Expected diagnostic file content for guarded cleanup')
p.add_argument('--button-provider-package', type=Path, help='Optional experimental provider INF, SYS and CAT')
p.add_argument('--button-provider-status', type=Path, help='Optional read-only provider PnP status executable')
p.add_argument('--touch-provider-package', type=Path, help='Optional experimental touch provider INF, SYS and CAT')
p.add_argument('--touch-provider-status', type=Path, help='Optional read-only touch PnP status executable')
p.add_argument('--touch-provider-original', type=Path, help='Expected active touch SYS for guarded replacement')
p.add_argument('--touch-provider-original-inf', type=Path, help='Exact currently installed touch INF for guarded replacement')
p.add_argument('--storage-upgrade-package', type=Path, help='MT8167 MSDC boot package to stage, without replacing WinPE storage')
p.add_argument('--storage-original', type=Path, help='Expected active legacy storage SYS for guarded replacement')
p.add_argument('--toprgu-package', type=Path, help='MT8167 TOPRGU watchdog test package')
p.add_argument('--toprgu-original', type=Path, help='Expected installed TOPRGU SYS for guarded upgrade')
p.add_argument('--toprgu-provider-status', type=Path, help='Read-only TOPRGU PnP status executable')
p.add_argument('--read-emmc-model', type=Path, help='Read-only volatile eMMC model utility')
p.add_argument('--disable-quickedit', type=Path, help='Optional signed console selection control executable')
p.add_argument('--lc709203f-test', type=Path, help='Signed LC709203F test package directory')
p.add_argument('--mma8652-clean-cert', type=Path, help='Certificate accompanying the clean-check helper')
p.add_argument('--mma8652-clean-check', type=Path, help='Signed NTFS dirty-bit check executable')
p.add_argument('--mma8652-package', type=Path, help='Test-signed MMA8652 diagnostic package')
p.add_argument('--mma8652-tested', type=Path, help='Separately sourced, verified WinPE-tested INF/SYS/CAT')
p.add_argument('--ge8300-display-test', type=Path, help='Paired signed GE8300 and display test package directory')
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
startnet = a.startnet or root / 'tools/winpe/startnet-driver-probe.cmd'
storage_boot = (a.package / 'mt8167-msdc.inf').is_file()
inf_name = 'mt8167-msdc.inf' if storage_boot else 'a04-msdc-probe.inf'
sys_name = 'mt8167-msdc.sys' if storage_boot else 'a04msdcprobe.sys'
cat_name = 'mt8167-msdc.cat' if storage_boot else 'a04msdcprobe.cat'
files = {'/Windows/System32/startnet.cmd': startnet,
         '/a04br3/a04-msdc-probe.inf': a.package / inf_name,
         f'/a04br3/{sys_name}': a.package / sys_name,
         f'/a04br3/{cat_name}': a.package / cat_name}
if a.guarded_test:
    files['/a04br3/guarded-write-test.exe'] = a.guarded_test
if a.bcd_export:
    files['/a04br3/guarded-bcd-export.exe'] = a.bcd_export
if a.button_inject:
    files['/a04br3/button-inject.exe'] = a.button_inject
if a.button_driver:
    for name in ('msgpiowin32.inf', 'msgpiowin32.sys'):
        files[f'/a04br3/button-driver/{name}'] = a.button_driver / name
if a.root_repair:
    files['/a04br3/repair-root.exe'] = a.root_repair
if a.volume_guard:
    files['/a04br3/guard-windows-volume.exe'] = a.volume_guard
if a.button_probe_service:
    files['/a04br3/button-probe-service.exe'] = a.button_probe_service
if a.button_probe_original:
    files['/a04br3/button-probe-original.exe'] = a.button_probe_original
if a.compare_files:
    files['/a04br3/compare-files.exe'] = a.compare_files
if a.button_kernel_probe:
    files['/a04br3/button-kernel-probe.sys'] = a.button_kernel_probe
if a.flush_probe_reference:
    files['/a04br3/flush-probe-reference.txt'] = a.flush_probe_reference
if a.button_provider_package:
    for name in ('mt8167-mt6392-buttons.inf', 'mt8167-mt6392-buttons.sys', 'mt8167-mt6392-buttons.cat'):
        files[f'/a04br3/button-provider/{name}'] = a.button_provider_package / name
if a.button_provider_status:
    files['/a04br3/button-provider-status.exe'] = a.button_provider_status
if a.touch_provider_package:
    for name in ('bu21025.inf', 'bu21025.sys', 'bu21025.cat'):
        files[f'/a04br3/touch-provider/{name}'] = a.touch_provider_package / name
if a.touch_provider_status:
    files['/a04br3/touch-provider-status.exe'] = a.touch_provider_status
if a.touch_provider_original:
    files['/a04br3/touch-provider-original.sys'] = a.touch_provider_original
if a.touch_provider_original_inf:
    files['/a04br3/touch-provider-original.inf'] = a.touch_provider_original_inf
if a.storage_upgrade_package:
    for name in ('mt8167-msdc.inf', 'mt8167-msdc.sys', 'mt8167-msdc.cat'):
        files[f'/a04br3/storage-upgrade/{name}'] = a.storage_upgrade_package / name
if a.storage_original:
    files['/a04br3/storage-original.sys'] = a.storage_original
if a.toprgu_package:
    for name in ('mt8167-toprgu.inf', 'mt8167-toprgu.sys', 'mt8167-toprgu.cat'):
        files[f'/a04br3/toprgu/{name}'] = a.toprgu_package / name
if a.toprgu_original:
    files['/a04br3/toprgu-original.sys'] = a.toprgu_original
if a.toprgu_provider_status:
    files['/a04br3/toprgu-provider-status.exe'] = a.toprgu_provider_status
if a.read_emmc_model:
    files['/a04br3/read-emmc-model.exe'] = a.read_emmc_model
if a.disable_quickedit:
    files['/a04br3/disable-quickedit.exe'] = a.disable_quickedit
if a.lc709203f_test:
    for name in ('lc709203f.inf', 'lc709203f.sys', 'lc709203f.cat', 'lc709203f-status.exe', 'lc709203f-pnp-status.exe'):
        files[f'/a04br3/lc709203f/{name}'] = a.lc709203f_test / name
if bool(a.mma8652_package) != bool(a.mma8652_tested):
    raise SystemExit('MMA8652 package and independent tested reference are both required')
if a.mma8652_package:
    # Byte identities extracted from the ESP used in the run3 hardware test.
    expected = {
        'mma8652.inf': '307031831046bb23862ddfc1d77338a0d7539551016fa2fccb27cb37ccd1a306',
        'mma8652.sys': 'a86c199ad77e9156b80e3eda493aadebc97c55fb49e52bc14bad549ae57534c5',
        'mma8652.cat': '67cee6946b765c57d59f2f7eedd0960ef5ad9624d49f7e8b3acaf56ebec7bfbc',
        'mma8652-status.exe': '24908d41ddec601a0f6809c208dd0ede3edc26266a6eaf57216f513b583325b3',
        'mma8652-read.exe': 'afc5987797c7326e37af5f429a3c833143e69f26a330ef5c70132f494cfb5b3b',
    }
    for name, sha in expected.items():
        with (a.mma8652_package / name).open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != sha:
                raise SystemExit(f'Not the run3 tested package: {name}')
    for name in ('mma8652.inf', 'mma8652.sys', 'mma8652.cat'):
        with (a.mma8652_tested / name).open('rb') as stream:
            if hashlib.file_digest(stream, 'sha256').hexdigest() != expected[name]:
                raise SystemExit(f'Not the run3 tested reference: {name}')
    if not a.volume_guard or not a.compare_files or not a.mma8652_clean_check or not a.mma8652_clean_cert:
        raise SystemExit('MMA8652 offline staging needs volume guard and comparison tool')
    files['/a04br3/check-volume-clean.exe'] = a.mma8652_clean_check
    files['/a04br3/check-volume-clean.cer'] = a.mma8652_clean_cert
    files['/a04br3/mma8652/verify-installed.cmd'] = root / 'tools/winpe/verify-mma8652-installed.cmd'
    for name in ('mma8652.inf', 'mma8652.sys', 'mma8652.cat', 'mma8652-status.exe', 'mma8652-read.exe'):
        files[f'/a04br3/mma8652/{name}'] = a.mma8652_package / name
    for name in ('mma8652.inf', 'mma8652.sys', 'mma8652.cat'):
        files[f'/a04br3/mma8652-tested/{name}'] = a.mma8652_tested / name
if a.ge8300_display_test:
    for name in ('mt8167-ge8300.inf', 'mt8167-ge8300.sys', 'mt8167-ge8300.cat',
                 'ge8300-mips.fw', 'ge8300-pnp-status.exe', 'ge8300-service-status.exe',
                 'ge8300-copy-status.exe', 'display-pnp-status.exe'):
        files[f'/a04br3/ge8300-display/ge8300/{name}'] = a.ge8300_display_test / 'ge8300' / name
    for name in ('mt8167-display.inf', 'mt8167-display.sys', 'mt8167-display.cat'):
        files[f'/a04br3/ge8300-display/display/{name}'] = a.ge8300_display_test / 'display' / name
for source in [a.source_wim, *files.values()]:
    if not source.is_file():
        raise SystemExit(f'Missing input: {source}')
    if any(c.isspace() or c in '"\'' for c in str(source.resolve())):
        raise SystemExit('wimlib update inputs must have paths without whitespace or quotes')
if a.output.exists():
    raise SystemExit('Output already exists')
a.output.parent.mkdir(parents=True, exist_ok=True)
with a.source_wim.open('rb') as source, a.output.open('xb') as target:
    shutil.copyfileobj(source, target, 1024 * 1024)
commands = ['delete --force /Windows/System32/winpeshl.ini']
commands.extend(f'add {source.resolve()} {dest}' for dest, source in files.items())
subprocess.run([a.wimlib, 'update', str(a.output), '1'], input='\n'.join(commands) + '\n', text=True, check=True)
subprocess.run([a.wimlib, 'verify', str(a.output)], check=True)
with tempfile.TemporaryDirectory(prefix='a04br3-wim-check-') as tmp:
    subprocess.run([a.wimlib, 'extract', str(a.output), '1', '/a04br3', '/Windows/System32/startnet.cmd', f'--dest-dir={tmp}'], check=True)
    for dest, original in files.items():
        rel = dest.removeprefix('/a04br3/') if dest.startswith('/a04br3/') else 'startnet.cmd'
        extracted = Path(tmp) / ('a04br3' if dest.startswith('/a04br3/') else '') / rel
        if extracted.read_bytes() != original.read_bytes():
            raise SystemExit(f'WIM comparison failed: {dest}')
print(f'Created and verified {a.output}')
