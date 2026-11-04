#!/usr/bin/env python3
"""Verify the staging builder rejects a tampered tested package before writes."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
p = argparse.ArgumentParser()
p.add_argument('--package', type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='a04-mma-guard-') as d:
    d = Path(d)
    shutil.copytree(a.package, d/'package')
    shutil.copytree(a.package, d/'reference')
    (d/'package'/'mma8652.sys').write_bytes(b'tampered')
    for n in ('dummy.wim', 'guard.exe', 'compare.exe', 'clean.exe', 'clean.cer'):
        (d/n).write_bytes(b'fixture')
    (d/'storage').mkdir()
    cmd=[sys.executable,str(root/'tools/winpe/prepare-driver-probe-wim.py'),
         '--source-wim',str(d/'dummy.wim'),'--package',str(d/'storage'),
         '--output',str(d/'out.wim'),'--volume-guard',str(d/'guard.exe'),
         '--compare-files',str(d/'compare.exe'),'--mma8652-clean-check',str(d/'clean.exe'),
         '--mma8652-clean-cert',str(d/'clean.cer'), '--mma8652-package',str(d/'package'),
         '--mma8652-tested',str(d/'reference')]
    result=subprocess.run(cmd,text=True,capture_output=True)
    assert result.returncode != 0 and 'Not the run3 tested package: mma8652.sys' in result.stderr
    assert not (d/'out.wim').exists()
print('MMA8652 tampered input rejected before WIM output')
