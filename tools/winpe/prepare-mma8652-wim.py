#!/usr/bin/env python3
"""Create an offline WinPE derivative with the MMA8652 PnP diagnostic."""
import argparse
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--wimlib', required=True)
p.add_argument('--source-wim', type=Path, required=True)
p.add_argument('--package', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
if a.output.exists():
    raise SystemExit('Refusing to overwrite output')
names = ('mma8652.inf', 'mma8652.sys', 'mma8652.cat', 'mma8652.cer', 'mma8652-status.exe', 'mma8652-read.exe')
for f in (a.source_wim, *(a.package / n for n in names)):
    if not f.is_file() or any(c.isspace() or c in '\"\'' for c in str(f.resolve())):
        raise SystemExit(f'Missing input or unsupported path: {f}')
a.output.parent.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='a04-mma-wim-') as temp:
    tmp = Path(temp)
    subprocess.run([a.wimlib, 'extract', str(a.source_wim), '1',
                    '/Windows/System32/startnet.cmd', f'--dest-dir={tmp}'], check=True)
    original = (tmp / 'startnet.cmd').read_bytes()
    marker = b'echo DISPLAY_TEST_FINISHED'
    if original.count(marker) != 1:
        raise SystemExit('Unexpected source startnet; refusing to alter it')
    extension = (b'\r\necho MMA8652_TEST_START\r\n'
                 b'drvload X:\\a04br3\\mma8652\\mma8652.inf\r\n'
                 b'echo MMA8652_DRVLOAD_EXIT=%errorlevel%\r\n'
                 b'X:\\a04br3\\mma8652\\mma8652-status.exe\r\n'
                 b'echo MMA8652_STATUS_EXIT=%errorlevel%\r\n'
                 b'X:\\a04br3\\mma8652\\mma8652-read.exe\r\n'
                 b'echo MMA8652_READ_EXIT=%errorlevel%\r\n'
                 b'echo MMA8652_TEST_FINISHED\r\n')
    startnet = tmp / 'startnet-new.cmd'
    startnet.write_bytes(original.rstrip(b'\r\n') + extension)
    shutil.copyfile(a.source_wim, a.output)
    files = {'/Windows/System32/startnet.cmd': startnet}
    files.update({f'/a04br3/mma8652/{n}': a.package / n for n in names})
    commands = '\n'.join(f'add {src.resolve()} {dest}' for dest, src in files.items()) + '\n'
    subprocess.run([a.wimlib, 'update', str(a.output), '1'], input=commands, text=True, check=True)
    subprocess.run([a.wimlib, 'verify', str(a.output)], check=True)
    check = tmp / 'check'
    subprocess.run([a.wimlib, 'extract', str(a.output), '1', '/a04br3/mma8652',
                    '/Windows/System32/startnet.cmd', f'--dest-dir={check}'], check=True)
    for dest, src in files.items():
        extracted = check / ('mma8652/' + Path(dest).name if dest.startswith('/a04br3/') else 'startnet.cmd')
        if extracted.read_bytes() != src.read_bytes():
            raise SystemExit(f'WIM verification mismatch: {dest}')
print('Created WIM:', a.output)
with a.output.open('rb') as stream:
    print('SHA256:', hashlib.file_digest(stream, 'sha256').hexdigest())
