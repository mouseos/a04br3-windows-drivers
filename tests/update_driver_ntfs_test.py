#!/usr/bin/env python3
"""Exercise device selection and read-only fallback without an actual device."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
fake = r'''#!/usr/bin/env python3
import hashlib, os, pathlib, shlex, shutil, sys
p = pathlib.Path(os.environ['A04_FAKE_ROOT'])
args = sys.argv[1:]
with (p / 'calls').open('a') as f: f.write(repr(args) + '\n')
if args[:2] != ['-s', 'target-serial']: sys.exit(90)
args = args[2:]
if args == ['get-state']: print('recovery')
elif args[:2] == ['shell', 'id']: print('0')
elif args[:3] == ['shell', 'getprop', 'ro.product.device']: print(os.environ.get('A04_FAKE_PRODUCT', 'a04br3'))
elif args[:2] == ['shell', 'readlink']: print('/dev/block/mmcblk0p20')
elif args[:2] == ['shell', 'cat']:
    name = args[2]
    if name.endswith('/start'): print('917504')
    elif name.endswith('/size'): print('29584384')
    elif name.endswith('/dev'): print('179:20')
    elif name == '/proc/mounts': print('/dev/block/mmcblk0p20 /tmp/a04-windows-update fuseblk ro,relatime 0 0')
elif args[0] == 'push':
    dest = p / 'remote' / pathlib.Path(args[2]).name
    dest.parent.mkdir(exist_ok=True); shutil.copyfile(args[1], dest)
elif args[0] == 'pull': pathlib.Path(args[2]).write_bytes(('backup:' + pathlib.Path(args[1]).name).encode())
elif args[:2] == ['shell', '-T']:
    command = args[2]
    if command.startswith('sha256sum '):
        name = shlex.split(command)[1]
        data = (p / 'inf').read_bytes() if '/a04-windows-update/' in name else (p / 'remote' / pathlib.Path(name).name).read_bytes()
        print(hashlib.sha256(data).hexdigest(), name)
    elif command.startswith(('mkdir ', 'chmod ', 'umount ', '/tmp/a04-driver-update/ntfs-3g ')): pass
    else: sys.exit(91)
else: sys.exit(92)
'''

with tempfile.TemporaryDirectory(prefix='a04-driver-update-test-') as tmp:
    p = Path(tmp)
    package = p / 'package'
    package.mkdir()
    for name in ('a04msdcprobe.sys', 'a04msdcprobe.cat', 'a04-msdc-probe.inf'):
        (package / name).write_bytes(('package:' + name).encode())
    (p / 'inf').write_bytes((package / 'a04-msdc-probe.inf').read_bytes())
    ntfs = p / 'ntfs-3g'
    ntfs.write_bytes(b'test-only executable')
    adb = p / 'adb'
    adb.write_text(fake)
    adb.chmod(0o755)
    env = dict(os.environ, PATH=str(p) + os.pathsep + os.environ['PATH'], A04_FAKE_ROOT=str(p))
    cmd = [sys.executable, str(root / 'tools/recovery/update-driver-ntfs.py'),
           '--serial', 'target-serial', '--package', str(package), '--ntfs3g', str(ntfs),
           '--backup-dir', str(p / 'backup')]
    result = subprocess.run(cmd, env=env, text=True, capture_output=True)
    assert result.returncode != 0 and '読取り専用' in result.stderr, result.stderr
    calls = (p / 'calls').read_text()
    assert "'-s', 'target-serial'" in calls and 'set -e; cp ' not in calls
    assert (p / 'backup' / 'manifest.json').is_file()
    (p / 'calls').unlink()
    env['A04_FAKE_PRODUCT'] = 'other-device'
    result = subprocess.run(cmd[:-1] + [str(p / 'wrong-backup')], env=env, text=True, capture_output=True)
    assert result.returncode != 0
    assert "'push'" not in (p / 'calls').read_text()
print('driver update selection and read-only fallback verified')
