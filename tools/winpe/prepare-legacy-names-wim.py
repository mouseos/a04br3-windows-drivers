#!/usr/bin/env python3
"""Prepare a guarded WinPE copy for legacy hardware-name servicing."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
for name in ('source', 'package', 'helper', 'helper-cert', 'uart-event', 'original-storage-inf', 'original-touch-inf', 'output'):
    parser.add_argument('--' + name, type=Path, required=True)
parser.add_argument('--source-sha256', required=True)
parser.add_argument('--wimlib', default='wimlib-imagex')
a = parser.parse_args()
root = Path(__file__).resolve().parents[2]

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def run(args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)

def header(path):
    with path.open('rb') as stream:
        data = stream.read(128)
    if len(data) != 128 or data[:8] != b'MSWIM\0\0\0':
        raise SystemExit('invalid WIM header')
    return struct.unpack_from('<I', data, 44)[0], struct.unpack_from('<I', data, 120)[0]

if a.output.exists() or a.output.with_suffix(a.output.suffix + '.json').exists():
    raise SystemExit('output already exists')
if sha(a.source) != a.source_sha256.lower() or header(a.source) != (1, 1):
    raise SystemExit('source hash or boot index mismatch')
files = {
    '/Windows/System32/startnet.cmd': root / 'tools/winpe/startnet-stage-legacy-names.cmd',
    '/a04br3/check-volume-clean.exe': a.helper,
    '/a04br3/uart-name-event.exe': a.uart_event,
    '/a04br3/names/original-storage.inf': a.original_storage_inf,
    '/a04br3/names/original-touch.inf': a.original_touch_inf,
}
for name in ('storage/a04msdcprobe.sys', 'storage/mt8167-msdc-legacy.inf', 'storage/mt8167-msdc-legacy.cat',
             'touch/a04touch.sys', 'touch/bu21025-legacy.inf', 'touch/bu21025-legacy.cat'):
    files['/a04br3/names/' + name] = a.package / name
for path in (*files.values(), a.source, a.helper_cert, a.package / 'legacy-names.cer', a.output):
    if any(c.isspace() or c in "\"'" for c in str(path.resolve())):
        raise SystemExit('paths containing whitespace or quotes are not supported')
for path in files.values():
    if not path.is_file():
        raise SystemExit('missing input: ' + str(path))
if sha(a.package / 'storage/a04msdcprobe.sys') != 'beb9d96b15b16ed5313f74f387832762167d69e61c237536441ed25189a1d58a' or sha(a.package / 'touch/a04touch.sys') != '3c1d86fe403b7c754326140d5331e1bce5a05ab95b6415f6381ece162d20fa8a':
    raise SystemExit('legacy driver hashes do not match the retained binaries')

with tempfile.TemporaryDirectory(prefix='a04-legacy-name-wim-') as directory:
    tmp = Path(directory)
    run([a.wimlib, 'extract', str(a.source), '1', '/Windows/System32/config/SOFTWARE', f'--dest-dir={tmp}'], stdout=subprocess.DEVNULL)
    hive = tmp / 'SOFTWARE'
    base = '\\Microsoft\\SystemCertificates'
    def hive_cmd(commands, write=False):
        result = run(['hivexsh', *(['-w'] if write else []), str(hive)], input=commands, text=True, capture_output=True)
        if result.stderr:
            raise SystemExit('registry update failed: ' + result.stderr)
        return result.stdout
    def ensure(parent, name):
        if name.casefold() not in {x.strip().casefold() for x in hive_cmd(f'cd {parent}\nls\n').splitlines()}:
            hive_cmd(f'cd {parent}\nadd {name}\ncommit\n', True)
    for store in ('Root', 'TrustedPublisher'):
        ensure(base, store)
        ensure(base + '\\' + store, 'Certificates')
    certificates = {}
    for cert in (a.helper_cert, a.package / 'legacy-names.cer'):
        run(['openssl', 'x509', '-inform', 'DER', '-in', str(cert), '-noout'], stdout=subprocess.DEVNULL)
        data = cert.read_bytes()
        thumb = hashlib.sha1(data).hexdigest().upper()
        blob = struct.pack('<III', 32, 1, len(data)) + data
        certificates[thumb] = blob
        for store in ('Root', 'TrustedPublisher'):
            path = f'{base}\\{store}\\Certificates'
            if thumb not in hive_cmd(f'cd {path}\nls\n').upper().splitlines():
                hive_cmd(f'cd {path}\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\ncommit\n', True)
            got = run(['hivexget', str(hive), path+'\\'+thumb, 'Blob'], capture_output=True).stdout
            if got != blob:
                raise SystemExit('WinPE certificate verification failed')
    files['/Windows/System32/config/SOFTWARE'] = hive
    a.output.parent.mkdir(parents=True, exist_ok=True)
    with a.source.open('rb') as source, a.output.open('xb') as target:
        shutil.copyfileobj(source, target, 1024 * 1024)
    commands = ''.join(f'add {src.resolve()} {dest}\n' for dest, src in files.items())
    run([a.wimlib, 'update', str(a.output), '1'], input=commands, text=True)
    if header(a.output) != (1, 1):
        raise SystemExit('output boot index changed')
    run([a.wimlib, 'verify', str(a.output)])
    check = tmp / 'check'
    run([a.wimlib, 'extract', str(a.output), '1', '/a04br3/names', '/a04br3/check-volume-clean.exe', '/a04br3/uart-name-event.exe', '/Windows/System32/startnet.cmd', '/Windows/System32/config/SOFTWARE', f'--dest-dir={check}'], stdout=subprocess.DEVNULL)
    for dest, src in files.items():
        name = dest.removeprefix('/a04br3/') if dest.startswith('/a04br3/') else Path(dest).name
        if sha(check / name) != sha(src):
            raise SystemExit('WIM extraction mismatch: ' + dest)
    manifest = {'source_sha256': a.source_sha256.lower(), 'wim_sha256': sha(a.output),
                'files': {dest: sha(src) for dest, src in files.items()},
                'winpe_test_certificates': list(certificates), 'boot_index': 1,
                'note': 'Offline staging only; installed OS trust, PnP selection and hardware names remain unverified.'}
    a.output.with_suffix(a.output.suffix + '.json').write_text(json.dumps(manifest, indent=2) + '\n')
print('Prepared', a.output)
