#!/usr/bin/env python3
"""Create a WinPE copy using one intact, already-built driver package."""
import argparse
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
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
startnet = a.startnet or root / 'tools/winpe/startnet-driver-probe.cmd'
files = {'/Windows/System32/startnet.cmd': startnet,
         '/a04br3/a04-msdc-probe.inf': a.package / 'a04-msdc-probe.inf',
         '/a04br3/a04msdcprobe.sys': a.package / 'a04msdcprobe.sys',
         '/a04br3/a04msdcprobe.cat': a.package / 'a04msdcprobe.cat'}
if a.guarded_test:
    files['/a04br3/guarded-write-test.exe'] = a.guarded_test
if a.bcd_export:
    files['/a04br3/guarded-bcd-export.exe'] = a.bcd_export
if a.button_inject:
    files['/a04br3/button-inject.exe'] = a.button_inject
if a.button_driver:
    for name in ('msgpiowin32.inf', 'msgpiowin32.sys'):
        files[f'/a04br3/button-driver/{name}'] = a.button_driver / name
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
commands = ['delete /Windows/System32/winpeshl.ini']
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
