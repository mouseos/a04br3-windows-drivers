#!/usr/bin/env python3
"""Build and verify a WinPE copy for the GE8300 firmware test."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
p = argparse.ArgumentParser()
p.add_argument('--wimlib', default='wimlib-imagex')
p.add_argument('--source-wim', type=Path, required=True)
p.add_argument('--package', type=Path, required=True)
p.add_argument('--disable-quickedit', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
files = {'/Windows/System32/startnet.cmd': root / 'tools/winpe/startnet-ge8300-firmware.cmd',
         '/Windows/System32/drivers/ge8300-mips.fw': a.package / 'ge8300-mips.fw',
         '/a04br3/disable-quickedit.exe': a.disable_quickedit}
for name in ('mt8167-ge8300.inf', 'mt8167-ge8300.sys', 'mt8167-ge8300.cat',
             'ge8300-mips.fw', 'ge8300-status.exe', 'ge8300-pnp-status.exe'):
    files[f'/a04br3/ge8300/{name}'] = a.package / name
for source in (a.source_wim, *files.values()):
    if not source.is_file():
        raise SystemExit(f'Missing input: {source}')
    if any(c.isspace() or c in '\"\'' for c in str(source.resolve())):
        raise SystemExit('wimlib input path contains whitespace or quotes')
if a.output.exists():
    raise SystemExit('Output already exists')
a.output.parent.mkdir(parents=True, exist_ok=True)
with a.source_wim.open('rb') as src, a.output.open('xb') as dst:
    shutil.copyfileobj(src, dst, 1024 * 1024)
commands = [f'add {source.resolve()} {dest}' for dest, source in files.items()]
subprocess.run([a.wimlib, 'update', str(a.output), '1'], input='\n'.join(commands)+'\n', text=True, check=True)
subprocess.run([a.wimlib, 'verify', str(a.output)], check=True)
with tempfile.TemporaryDirectory(prefix='a04br3-gpu-wim-') as temp:
    tmp = Path(temp)
    subprocess.run([a.wimlib, 'extract', str(a.output), '1', '/a04br3/ge8300',
                    '/a04br3/disable-quickedit.exe', '/Windows/System32/startnet.cmd',
                    '/Windows/System32/drivers/ge8300-mips.fw', f'--dest-dir={tmp}'], check=True)
    for dest, source in files.items():
        if dest.startswith('/a04br3/ge8300/'):
            extracted = tmp / 'ge8300' / Path(dest).name
        else:
            extracted = tmp / Path(dest).name
        if extracted.read_bytes()!=source.read_bytes():
            raise SystemExit(f'WIM comparison failed: {dest}')
print(f'Created and verified {a.output}')
