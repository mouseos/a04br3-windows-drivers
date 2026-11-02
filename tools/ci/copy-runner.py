#!/usr/bin/env python3
"""SFTP transfer through the authenticated, ephemeral Windows runner tunnel."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('run_id', type=int)
p.add_argument('direction', choices=('upload', 'download'))
p.add_argument('local', type=Path)
p.add_argument('remote', help='Absolute Windows path using forward slashes, without whitespace')
a = p.parse_args()
if not re.fullmatch(r'[A-Za-z]:/(?:[A-Za-z0-9_.-]+/)*[A-Za-z0-9_.-]+', a.remote):
    raise SystemExit('Unsupported remote path')
if any(part in ('.', '..') for part in a.remote.split('/')):
    raise SystemExit('Remote path traversal is not allowed')
root = Path(__file__).resolve().parents[2]
jump_hosts = root / '.github/actions/open-ssh/ssh-j-known-hosts'
run_id = str(a.run_id)
partial = a.local.with_name(a.local.name + '.partial')
if a.direction == 'upload' and not a.local.is_file():
    raise SystemExit('Local input is missing')
if a.direction == 'download' and (a.local.exists() or partial.exists()):
    raise SystemExit('Local output or partial output already exists')
with tempfile.TemporaryDirectory(prefix='a04br3-runner-copy-') as directory:
    subprocess.run(['gh', 'run', 'download', run_id, '--repo', 'mouseos/a04br3-windows-drivers',
                    '--name', f'a04br3-ssh-{run_id}', '--dir', directory], check=True)
    data = json.loads((Path(directory) / 'a04br3-ssh.json').read_text(encoding='utf-8-sig'))
    if (data['run_id'] != run_id or data['jump_host'] != 'ssh-j.com' or
            data['jump_user'] != f'a04br3-{run_id}' or data['alias'] != f'wdk-{run_id}' or
            data['port'] != 2222 or not re.fullmatch(r'[A-Za-z0-9_.-]+', data['user'])):
        raise SystemExit('Unexpected connection metadata')
    lines = data['host_keys'].splitlines()
    if not lines or any(not re.fullmatch(r'(?:ssh-(?:ed25519|rsa)|ecdsa-sha2-nistp(?:256|384|521)) [A-Za-z0-9+/=]+(?: .*)?', line) for line in lines):
        raise SystemExit('Invalid runner host keys')
    known = Path(directory) / 'known_hosts'
    known.write_text(''.join(f"[{data['alias']}]:2222 {line}\n" for line in lines))
    proxy = (f'ssh -o BatchMode=yes -o StrictHostKeyChecking=yes '
             f'-o UserKnownHostsFile={shlex.quote(str(jump_hosts))} -W %h:%p {data["jump_user"]}@ssh-j.com')
    remote = f"{data['user']}@{data['alias']}:{a.remote}"
    source, target = (str(a.local), remote) if a.direction == 'upload' else (remote, str(partial))
    subprocess.run(['scp', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes',
                    '-o', f'UserKnownHostsFile={known}', '-o', f'ProxyCommand={proxy}',
                    '-P', '2222', source, target], check=True)
    if a.direction == 'download':
        partial.rename(a.local)
