#!/usr/bin/env python3
"""Connect to a running Windows CI job using its authenticated artifact."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('run_id', type=int)
parser.add_argument('command', nargs=argparse.REMAINDER)
args = parser.parse_args()
repo = 'mouseos/a04br3-windows-drivers'
root = Path(__file__).resolve().parents[2]
jump_hosts = root / '.github/actions/open-ssh/ssh-j-known-hosts'
with tempfile.TemporaryDirectory(prefix='a04br3-runner-') as directory:
    subprocess.run(['gh', 'run', 'download', str(args.run_id), '--repo', repo,
                    '--name', f'a04br3-ssh-{args.run_id}', '--dir', directory], check=True)
    data = json.loads((Path(directory) / 'a04br3-ssh.json').read_text(encoding='utf-8-sig'))
    if (data['run_id'] != str(args.run_id) or data['jump_host'] != 'ssh-j.com'
            or data['jump_user'] != f'a04br3-{args.run_id}'
            or data['alias'] != f'wdk-{args.run_id}' or data['port'] != 2222
            or not re.fullmatch(r'[A-Za-z0-9_.-]+', data['user'])):
        raise SystemExit('Unexpected connection metadata')
    lines = data['host_keys'].splitlines()
    if not lines or any(not re.fullmatch(r'(?:ssh-(?:ed25519|rsa)|ecdsa-sha2-nistp(?:256|384|521)) [A-Za-z0-9+/=]+(?: .*)?', line) for line in lines):
        raise SystemExit('Invalid runner host keys')
    known = Path(directory) / 'known_hosts'
    known.write_text(''.join(f"[{data['alias']}]:2222 {line}\n" for line in lines))
    proxy = (f'ssh -o BatchMode=yes -o StrictHostKeyChecking=yes '
             f'-o UserKnownHostsFile={shlex.quote(str(jump_hosts))} -W %h:%p {data["jump_user"]}@ssh-j.com')
    cmd = ['ssh', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes',
           '-o', f'UserKnownHostsFile={known}', '-o', f'ProxyCommand={proxy}',
           '-p', '2222', f'{data["user"]}@{data["alias"]}', *args.command]
    raise SystemExit(subprocess.call(cmd))
