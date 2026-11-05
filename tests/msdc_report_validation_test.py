#!/usr/bin/env python3
import hashlib
import subprocess
import sys
import tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
tool = root / 'tools/recovery/validate-msdc-read-report.py'
with tempfile.TemporaryDirectory(dir=root / 'build/tmp') as td:
    d = Path(td); ref = d / 'reference'; ref.write_bytes(b'x' * (16 * 1024 * 1024))
    sha = hashlib.sha256(ref.read_bytes()).hexdigest()
    def report(n, elapsed, digest=sha):
        return f'MSDC_READ_STRESS_STARTED\nMSDC_READ_STRESS_PAIRS={n} ELAPSED_MS={elapsed}\nMSDC_REFERENCE_SHA256={digest}\nMSDC_READ_STRESS_PASSED\n'
    one=d/'single';two=d/'multi';one.write_text(report(16384,30000));two.write_text(report(16384,14000))
    cmd=[sys.executable,str(tool),'--reference',str(ref),'--single-report',str(one),'--multi-report',str(two)]
    assert subprocess.run(cmd,capture_output=True).returncode==0
    two.write_text(report(16384,14000,'0'*64));assert subprocess.run(cmd,capture_output=True).returncode!=0
    two.write_text(report(16384,14000)+report(16384,14000));assert subprocess.run(cmd,capture_output=True).returncode!=0
    two.write_text(one.read_text());assert subprocess.run(cmd,capture_output=True).returncode!=0
    two.write_text(report(16384,14000));ref.write_bytes(b'x');assert subprocess.run(cmd,capture_output=True).returncode!=0
print('MSDC report verification: matched reference, mismatches, duplicates and short reference tested')
