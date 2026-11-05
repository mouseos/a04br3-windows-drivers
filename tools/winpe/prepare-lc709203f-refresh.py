#!/usr/bin/env python3
"""Refresh the signed LC709203F package in a verified WinPE copy; no device I/O."""
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import tempfile
import zipfile
from pathlib import Path

BASE_SHA = '99c947f61fe0d8142d66938c516d2317dde0481def30d620c9f90e5b5961efb7'
PACKAGE_SHA = '22ce3a496ebc4ef9589cddb8d903074a6349174d7e055b8bdb0bf1655bb0d22a'
NAMES = ('lc709203f.inf', 'lc709203f.sys', 'lc709203f.cat',
         'lc709203f-status.exe', 'lc709203f-pnp-status.exe', 'lc709203f-test.cer')

def sha(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()

def run(args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    a = parser.parse_args()
    if sha(a.source) != BASE_SHA or sha(a.package) != PACKAGE_SHA:
        raise SystemExit('tested WIM or signed package hash mismatch')
    manifest = json.loads(a.manifest.read_text())
    if manifest.get('commit') != 'dcbb0d088674647c51a8031536792c3f76b77b5f' or manifest.get('zip_sha256') != PACKAGE_SHA:
        raise SystemExit('package provenance mismatch')
    if a.output.exists() or a.output.with_suffix(a.output.suffix + '.json').exists():
        raise SystemExit('output already exists')
    if any(any(c.isspace() or c in "\"'" for c in str(x.resolve())) for x in (a.source, a.output)):
        raise SystemExit('source and output paths must not contain whitespace or quotes')
    with tempfile.TemporaryDirectory(prefix='a04-lc709203f-') as tmp:
        tmp = Path(tmp)
        with zipfile.ZipFile(a.package) as archive:
            entries = {x.filename.replace('\\', '/'): x for x in archive.infolist() if not x.is_dir()}
            wanted = {'lc709203f-test-signed/' + n for n in NAMES}
            if set(entries) != wanted:
                raise SystemExit('unexpected package members')
            for name in NAMES:
                content = archive.read(entries['lc709203f-test-signed/' + name])
                key = 'lc709203f-test-signed\\' + name
                if hashlib.sha256(content).hexdigest() != manifest['files'].get(key):
                    raise SystemExit('package member hash mismatch: ' + name)
                (tmp / name).write_bytes(content)
        cert = (tmp / 'lc709203f-test.cer').read_bytes()
        run(['openssl', 'x509', '-inform', 'DER', '-in', str(tmp / 'lc709203f-test.cer'), '-noout'])
        thumb = hashlib.sha1(cert).hexdigest().upper()
        blob = struct.pack('<III', 32, 1, len(cert)) + cert
        run(['wimlib-imagex', 'extract', str(a.source), '1', '/Windows/System32/config/SOFTWARE', f'--dest-dir={tmp}'])
        hive = tmp / 'SOFTWARE'
        base = '\\Microsoft\\SystemCertificates'
        stores = [f'{base}\\{x}\\Certificates' for x in ('Root', 'TrustedPublisher')]
        # The tested WinPE lacks a TrustedPublisher store. Create only the
        # missing registry keys and retain every pre-existing certificate.
        def ensure_child(parent, name):
            listing = run(['hivexsh', str(hive)], input=f'cd {parent}\nls\n', text=True, capture_output=True)
            if listing.stderr:
                raise SystemExit('certificate registry parent missing: ' + parent)
            if name.casefold() not in {x.strip().casefold() for x in listing.stdout.splitlines()}:
                edit = run(['hivexsh', '-w', str(hive)], input=f'cd {parent}\nadd {name}\ncommit\n', text=True, capture_output=True)
                if edit.stderr:
                    raise SystemExit('certificate registry key creation failed')
        for name in ('Root', 'TrustedPublisher'):
            ensure_child(base, name)
            ensure_child(base + '\\' + name, 'Certificates')
        for store in stores:
            listing = run(['hivexsh', str(hive)], input=f'cd {store}\nls\n', text=True, capture_output=True)
            if listing.stderr or thumb in listing.stdout:
                raise SystemExit('certificate store missing or already contains certificate')
            commands = f'cd {store}\nadd {thumb}\ncd {thumb}\nsetval 1\nBlob\nhex:3:{blob.hex()}\ncommit\n'
            edit = run(['hivexsh', '-w', str(hive)], input=commands, text=True, capture_output=True)
            if edit.stderr:
                raise SystemExit('certificate store update failed')
            if run(['hivexget', str(hive), store+'\\'+thumb, 'Blob'], capture_output=True).stdout != blob:
                raise SystemExit('certificate store verification failed')
        a.output.parent.mkdir(parents=True, exist_ok=True)
        with a.source.open('rb') as source, a.output.open('xb') as target:
            shutil.copyfileobj(source, target, 1048576)
        commands = [f'add {hive} /Windows/System32/config/SOFTWARE']
        commands += [f'add {tmp / name} /a04br3/lc709203f/{name}' for name in NAMES if not name.endswith('.cer')]
        run(['wimlib-imagex', 'update', str(a.output), '1'], input='\n'.join(commands)+'\n', text=True)
        run(['wimlib-imagex', 'verify', str(a.output)])
        check = tmp / 'verify'; check.mkdir()
        run(['wimlib-imagex', 'extract', str(a.output), '1', '/Windows/System32/config/SOFTWARE',
             '/a04br3/lc709203f', f'--dest-dir={check}'])
        for name in NAMES:
            if not name.endswith('.cer') and sha(check / 'lc709203f' / name) != sha(tmp / name):
                raise SystemExit('WIM package verification failed: ' + name)
        for store in stores:
            if run(['hivexget', str(check / 'SOFTWARE'), store+'\\'+thumb, 'Blob'], capture_output=True).stdout != blob:
                raise SystemExit('WIM certificate verification failed')
        result = {'source_wim_sha256': BASE_SHA, 'package_sha256': PACKAGE_SHA,
                  'driver_commit': manifest['commit'], 'wim_sha256': sha(a.output),
                  'certificate_sha1': thumb, 'boot_index': 1,
                  'limitations': ['new driver and signature trust are not validated on the device',
                                  'baseline WinPE is diagnostic and not Windows installation media']}
        a.output.with_suffix(a.output.suffix + '.json').write_text(json.dumps(result, indent=2)+'\n')
        print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
