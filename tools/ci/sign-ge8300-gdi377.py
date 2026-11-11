#!/usr/bin/env python3
"""Sign actual two-TU GDI377 render only; no compile, link, install or device I/O."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import zipfile
from ge8300_panel_signing_common import require, decode_inf, unchanged_signed_payload

ROOT = Path(__file__).resolve().parents[2]
THUMB = 'BCBF6C456C6F0DD0818434763B189488FBF13570'


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))


def pin(path, digest):
    require(sha(path) == digest.lower(), 'Input hash mismatch: ' + str(path))


def child(root, name):
    # Manifest output names were produced on Windows; reject escaping paths.
    path = (root / name.replace('\\', '/')).resolve()
    require(path.is_relative_to(root.resolve()), 'Manifest path escapes input')
    return path


def inf377(source, baseline):
    text, encoding, bom = decode_inf(source)
    logical = text.replace('\r\n', '\n').replace('\r', '\n')
    oldtext = decode_inf(baseline)[0].replace('\r\n', '\n').replace('\r', '\n')
    dates = re.findall(r'^DriverVer=(11/(?:10|11)/2026),0\.0\.3\.77$', logical, re.M)
    require(len(dates)==1, 'Exact source377 DriverVer required')
    require(oldtext.count('DriverVer=11/10/2026,0.0.3.72')==1 and
            logical==oldtext.replace('DriverVer=11/10/2026,0.0.3.72', 'DriverVer='+dates[0]+',0.0.3.77'), 'Only DriverVer may change from baseline372 INF')
    require(datetime.date.today()>=datetime.datetime.strptime(dates[0],'%m/%d/%Y').date(), 'Source377 INF date is in the runner future; do not change clock')
    return bom + logical.replace('\n', '\r\n').encode(encoding)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--enable-candidate', action='store_true')
    p.add_argument('--baseline-package', type=Path, required=True)
    p.add_argument('--linked-directory', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--sign-tool', type=Path, required=True)
    p.add_argument('--inf2cat', type=Path, required=True)
    p.add_argument('--validate-only', action='store_true')
    a = p.parse_args()
    require(a.enable_candidate, 'Explicit candidate opt-in required')
    require(not subprocess.check_output(['git', '-C', ROOT, 'status', '--porcelain']), 'Clean recipe source required')
    commit = subprocess.check_output(['git', '-C', ROOT, 'rev-parse', 'HEAD'], text=True).strip()
    # Same fixed public tree as the successful build; no placeholder runtime commit.
    pins = read(Path(__file__).with_name('ge8300-gdi377-incremental-pins.json'))
    signing = pins['signing_tools']
    base, linked, out = a.baseline_package.resolve(), a.linked_directory.resolve(), a.output.resolve()
    pin(base / 'signed-manifest.json', pins['inputs']['package372/signed-manifest.json'])
    baseline = read(base / 'signed-manifest.json')
    require(baseline['success'] and baseline['signed'] and baseline['new_display_source_commit'] == pins['retained372_source'], 'Signed372 baseline identity mismatch')
    before = {r['name']: r['sha256'] for r in baseline['outputs']}
    require(len(before) == len(baseline['outputs']), 'Duplicate baseline output')
    for name, digest in before.items():
        pin(child(base, name), digest)
    build = read(linked / 'incremental-build-manifest.json')
    require(build['success'] and build['source_commit'] == commit and not build.get('executed') is False, 'Successful377 build from exact signing source required')
    expected = {r['object'] for r in pins['rows']}
    require(len(build['new_compiles']) == 2 and {r['object'] for r in build['new_compiles']} == expected, 'Exact two changed TU required')
    require(build['native_changed_TU'] == 0 and build['module_changed_TU'] == 2 and build['SYS_links'] == 1 and
            build['UMD_compiles'] == 0 and build['SDK_builds'] == 0 and not build['full_native_rebuilt'] and not build['transport_rebuilt'], 'Unexpected rebuild provenance')
    require(build['schema']=='ge8300-gdi377-two-tu-build-v1' and build['candidate_version']==377 and
            build['baseline376_manifest_sha256']==pins['baseline376_manifest_sha256'], 'Actual376-to377 build lineage required')
    require(build['reused_archives']['kmd']['replacement_members']==sorted(expected) and
            build['reused_archives']['native']['replacement_members']==[] and
            build['reused_archives']['native']['baseline_sha256']==build['reused_archives']['native']['output_sha256']==pins['baseline376_archives']['ge8300-native-owner-core.lib'], 'Only two KMD objects may change; native376 bytes retained')
    require({r['source'] for r in build['new_compiles']}=={r['source'] for r in pins['rows']},'Two exact source TUs required')
    for row in build['new_compiles']:
        source = child(ROOT, row['source'])
        pin(source, row['source_sha256'])
        pin(linked / 'objects' / row['object'], row['object_sha256'])
    for name, digest in build['outputs'].items():
        pin(child(linked, name), digest)
    require(build['retained_package_manifest_sha256'] == sha(base / 'signed-manifest.json') and
            build['retained_transport_sha256'] == sha(base / 'display-transport/mt8167-display-transport.sys'), 'Retained tuple binding mismatch')
    payload = (linked / 'ge8300-render.sys').read_bytes()
    source_inf=ROOT/'drivers/gpu/mt8167/wddm-render/ge8300-render.inf'
    updated_inf = inf377(source_inf.read_bytes(),(base / 'ge8300-render.inf').read_bytes())
    inf_driverver=re.search(r'^DriverVer=(.+)$',decode_inf(updated_inf)[0],re.M).group(1).strip()
    if a.validate_only:
        print(json.dumps(dict(validated=True, runtime_source_commit=commit, recipe_source_commit=commit,
                              build_and_sign_source_commit_equal=True, compile_count=0, link_count=0, executed=False)))
        return
    require(os.name == 'nt', 'Windows signing only')
    powershell = Path(shutil.which('powershell') or '')
    for name, tool in [('signtool.exe', a.sign_tool), ('Inf2Cat.exe', a.inf2cat), ('powershell.exe', powershell)]:
        pin(tool, signing[name])
    require(not out.exists() and out.is_relative_to(ROOT), 'Fresh source-local signing output required')
    out.mkdir(parents=True); (out / 'evidence').mkdir(); package = out / 'package377'; package.mkdir()
    receipt = dict(schema='ge8300-gdi377-render-sign-only-v1', recipe_source_commit=commit,
                   kernel_binary_source_commit=commit, runtime_source_commit=commit,
                   build_and_sign_source_commit_equal=True, runtime_source_tree_unchanged=True,
                   input_build_manifest_sha256=sha(linked / 'incremental-build-manifest.json'),
                   baseline372_manifest_sha256=sha(base / 'signed-manifest.json'),
                   inf_driverver=inf_driverver, source_inf_sha256=sha(source_inf), inf_normalization='CRLF only; source DriverVer preserved', signer_thumbprint=THUMB,
                   compile_count=0, link_count=0, input_build_compile_count=2, input_build_link_count=1, SDK_rebuilt=False, UMD_rebuilt=False,
                   transport_rebuilt=False, firmware_rebuilt=False, native_rebuilt=False, retained_native376_sha256=pins['baseline376_archives']['ge8300-native-owner-core.lib'],
                   signed=False, success=False, hardware_verified=False, deployment_performed=False, receipts=[])

    def run(args, name):
        q = subprocess.run(list(map(str, args)), capture_output=True, text=True, errors='replace')
        log = out / 'evidence' / name; log.write_text(q.stdout + q.stderr)
        receipt['receipts'].append(dict(arguments=list(map(str, args)), returncode=q.returncode,
                                        log=log.relative_to(out).as_posix(), sha256=sha(log)))
        require(q.returncode == 0, 'Signing/verification failed: ' + name)

    try:
        for name in before:
            dest = child(package, name); dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(child(base, name), dest)
        render = package / 'ge8300-render.sys'; render.write_bytes(payload)
        (package / 'ge8300-render.inf').write_bytes(updated_inf)
        (package / 'ge8300-render.cat').unlink()
        run([a.sign_tool, 'sign', '/fd', 'SHA256', '/s', 'My', '/sha1', THUMB, render], 'render.sign.log')
        unchanged_signed_payload(payload, render.read_bytes())
        # Isolate retained transport from Inf2Cat; preserve its INF/SYS/CAT
        # byte for byte, rather than recataloging or resigning version372.
        retained = out / 'retained-transport-work'
        (package / 'display-transport').rename(retained)
        try:
            run([a.inf2cat, '/driver:' + str(package), '/os:6_3_ARM', '/uselocaltime'], 'render.inf2cat.log')
        finally:
            retained.rename(package / 'display-transport')
        run([a.sign_tool, 'sign', '/fd', 'SHA256', '/s', 'My', '/sha1', THUMB, package / 'ge8300-render.cat'], 'render.cat.sign.log')
        signatures = [package / n for n in ('ge8300-render.sys', 'ge8300-render.cat', 'ge8300-umd.dll',
                      'display-transport/mt8167-display-transport.sys', 'display-transport/mt8167-display-transport.cat')]
        for i, path in enumerate(signatures):
            run([a.sign_tool, 'verify', '/pa', '/v', path], str(i) + '.signature.log')
        for folder, cat, names in [('', 'ge8300-render.cat', ('ge8300-render.inf', 'ge8300-render.sys', 'ge8300-umd.dll', 'ge8300-mips.fw')),
                                   ('display-transport', 'mt8167-display-transport.cat', ('mt8167-display-transport.inf', 'mt8167-display-transport.sys'))]:
            for name in names:
                run([a.sign_tool, 'verify', '/pa', '/v', '/c', package / folder / cat, package / folder / name], name + '.catalog.log')
        paths = out / 'evidence/signature-paths.json'; paths.write_text(json.dumps(list(map(str, signatures))) + '\n')
        report = out / 'evidence/certificate-report.json'
        run([powershell, '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File',
             ROOT / 'tools/ci/verify-dwm-certificates.ps1', '-PathsJson', paths, '-ExpectedThumbprint', THUMB, '-ReportJson', report], 'certificate-identity.log')
        identities = read(report)['files']
        require(len(identities) == 5 and {r['path'] for r in identities} == set(map(str, signatures)), 'Five exact BCBF certificate receipts required')
        actual = {f.relative_to(package).as_posix(): sha(f) for f in package.rglob('*') if f.is_file()}
        require(set(actual) == set(before), 'Final package file-set drift')
        changed = {'ge8300-render.sys', 'ge8300-render.inf', 'ge8300-render.cat'}
        for name, digest in actual.items():
            require((digest != before[name]) if name in changed else (digest == before[name]), 'Unexpected tuple change: ' + name)
        for name, digest in before.items():
            pin(child(base, name), digest)
        for name, digest in build['outputs'].items():
            pin(child(linked, name), digest)
        require(not subprocess.check_output(['git', '-C', ROOT, 'status', '--porcelain']) and
                subprocess.check_output(['git', '-C', ROOT, 'rev-parse', 'HEAD'], text=True).strip() == commit, 'Recipe source changed during signing')
        receipt.update(signed=True, success=True, retained_transport372_UMD_FW_clients=True,
                       binary_signing_transforms=[dict(name='ge8300-render.sys', unsigned_sha256=hashlib.sha256(payload).hexdigest(),
                                                       signed_sha256=sha(render), non_signature_payload_unchanged=True)],
                       outputs=[dict(name=n, sha256=h) for n, h in sorted(actual.items())])
        (package / 'signed-manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
    except BaseException as error:
        receipt['error'] = str(error)
        raise
    finally:
        (out / 'signing-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        (out / 'output-file-hashes.json').write_text(json.dumps({f.relative_to(out).as_posix(): sha(f) for f in out.rglob('*') if f.is_file()}, indent=2) + '\n')
        with zipfile.ZipFile(out.with_suffix('.zip'), 'w', zipfile.ZIP_DEFLATED) as archive:
            for file in out.rglob('*'):
                if file.is_file():
                    archive.write(file, file.relative_to(out))
    print('GDI377_RENDER_SIGNED_TRANSPORT372_UMD_FW_RETAINED_NO_DEVICE_OPERATION')


if __name__ == '__main__':
    main()
