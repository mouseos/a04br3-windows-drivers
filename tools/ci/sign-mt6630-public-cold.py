#!/usr/bin/env python3
"""公開cold43TU出力を検証し、既存Windows証明書storeだけで署名する。"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PureWindowsPath
import re
import shutil
import struct
import subprocess
from ge8300_panel_signing_common import decode_inf, pe, require, unchanged_signed_payload

ROOT = Path(__file__).resolve().parents[2]
HERE = ROOT / 'tools/ci'
NAME = 'mt6630-combo-runtime'


def normalize_thumbprint(value):
    require(isinstance(value, str) and re.fullmatch(r'[0-9a-fA-F]{40}', value) is not None, 'Thumbprint must be exactly 40 ASCII hex characters')
    return value.upper()


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def read(p):
    return json.loads(p.read_text(encoding='utf-8-sig'))


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def blob(commit, path):
    return git('show', commit + ':' + path)


def checkout_matches(data, digest):
    # Git text checkout conversion is the only accepted source byte variation.
    lf = data.replace(b'\r\n', b'\n')
    return digest in {hashlib.sha256(b).hexdigest() for b in (data, lf, lf.replace(b'\n', b'\r\n'))}


def validate(cold, expected_manifest, compiled):
    manifest = cold / 'unsigned-manifest.json'
    require(sha(manifest) == expected_manifest, 'Explicit cold manifest SHA mismatch')
    m = read(manifest)
    require(m['schema'] == 'mt6630-combo-cold-unsigned-v1', 'Cold unsigned schema required')
    require(re.fullmatch('[0-9a-f]{40}', compiled) and m['source_commit'] == compiled, 'Compiled source commit mismatch')
    require(m['source_dirty'] is False and m['new_compiled_units'] == 43 and m['reused_units'] == 0, 'Clean cold43, no reused objects')
    require(m['signed'] is False and m['private_baseline_required'] is False and m['SDK_setup'] is False, 'Unsigned source cold only')
    require(m['version'] == '2026.11.11.1033' and m['driver_date'] == '11/11/2026' and m['driver_ver'] == '11/11/2026,2026.11.11.1033', 'Formal1033 tuple')
    plan = read(HERE / 'mt6630-combo-cold-plan.json')
    require(json.loads(blob(compiled, 'tools/ci/mt6630-combo-cold-plan.json')) == plan, 'Compiled/current public plan differs')
    require(checkout_matches(blob(compiled, 'tools/ci/mt6630-combo-cold-plan.json'), m['plan_sha256']), 'Original plan hash')
    require(checkout_matches(blob(compiled, 'tools/ci/build-mt6630-combo-cold.ps1'), m['recipe_sha256']), 'Original cold recipe hash')
    for scope in ('drivers/network/mt6630', 'drivers/bluetooth/mt6630', 'drivers/bus/mt8167-shared'):
        changes = git('diff', '--name-only', compiled, 'HEAD', '--', scope).decode().splitlines()
        require(all(p.endswith('.md') for p in changes), 'Signer/runtime source tree differs: ' + scope)
    require(len(m['objects']) == 43 and [{'source': x['source'], 'object': x['object']} for x in m['objects']] == plan['objects'], 'Exact43 source/object order')
    # These public SDK/tool hashes are not a driver baseline or retained object input.
    spec = read(HERE / 'mt6630-provider1032-inputs.json')
    require(len(m['tools']) == 2 and [x['sha256'] for x in m['tools']] == [spec['tools']['clang'], spec['tools']['link']], 'Cold compiler/linker tuple')
    require(len(m['libraries']) == 5 and [x['sha256'] for x in m['libraries']] == [x['sha256'] for x in spec['kernel_libraries']], 'Five WDK library hashes/order')
    for x, pin in zip(m['libraries'], spec['kernel_libraries']):
        require(PureWindowsPath(x['path']).name == PureWindowsPath(pin['path']).name, 'Library identity')
    require(len(m['commands']) == 44 and len(m['include_roots']) == 4, '43 compile commands and one link')
    first = m['commands'][0]['arguments']
    i = first.index('-c')
    source_path = PureWindowsPath(first[i + 1])
    old_root = source_path
    for _ in PureWindowsPath(plan['objects'][0]['source']).parts:
        old_root = old_root.parent
    old_out = PureWindowsPath(first[-1]).parents[2]
    require(old_root.is_absolute() and old_out.is_relative_to(old_root / 'build'), 'Original checkout/output roots')
    includes = m['include_roots']
    require(all(PureWindowsPath(p).is_absolute() for p in includes), 'Absolute original include roots')
    require([PureWindowsPath(p).name.lower() for p in includes[:3]] == ['km', 'shared', 'um'] and PureWindowsPath(includes[3]).as_posix().lower().endswith('/wdf/kmdf/1.13'), 'WDK/WDF include roles')
    flags = ['-I' + str(old_out / 'ndis-overlay'), '--target=thumbv7-pc-windows-msvc', '-std=c11', '-fms-extensions', '-ffreestanding', '-O2', '-fno-vectorize', '-fno-slp-vectorize', '-mno-unaligned-access', '-mfpu=none', '-fno-stack-protector', '-ffunction-sections', '-fdata-sections', '-w', '-D__ldrexd=__combo_wdk81_ldrexd', '-D_KERNEL_MODE', '-D_ARM_', '-DARM', '-DNTDDI_VERSION=0x06030000', '-D_WIN32_WINNT=0x0603', '-DKMDF_VERSION_MAJOR=1', '-DKMDF_VERSION_MINOR=13']
    for p in includes:
        flags += ['-isystem', p]
    flags += ['-D_WINSOCK_DEPRECATED_NO_WARNINGS=1', '-DNDIS_MINIPORT_DRIVER', '-DNDIS630_MINIPORT', '-DA04_WIFI_SESSION_DIAGNOSTIC=1', '-DA04_WIFI_IRQ_DIAGNOSTIC=1', '-DA04_WIFI_CALIBRATION_DIAGNOSTIC=1', '-DA04_WIFI_BT_BROKER=1']
    require(m['flags'] == flags, 'Exact cold scalar/NDIS630/diagnostic/broker flags')
    locks = {manifest: sha(manifest)}
    ordered_objects = []
    for x, cmd in zip(m['objects'], m['commands'][:43]):
        require(x['compiled'] is True and x['compiled_source_commit'] == compiled, 'Do not relabel original compile provenance')
        rel = x['source']
        require(blob(compiled, rel) == blob('HEAD', rel), 'Current signer source differs from compiled TU: ' + rel)
        require(checkout_matches(blob(compiled, rel), x['source_sha256']), 'Compiled source hash: ' + rel)
        obj = cold / 'obj' / x['object']
        require(obj.stat().st_size == x['bytes'] and sha(obj) == x['sha256'] and struct.unpack_from('<H', obj.read_bytes())[0] == 0x1c4, 'Completed ARMNT object: ' + rel)
        dep = Path(str(obj) + '.d')
        require(sha(dep) == x['dependency_sha256'], 'Actual dependency file hash')
        original_obj = str(old_out / 'obj' / x['object'])
        require(x['dependency_file'] == original_obj + '.d', 'Original dependency path')
        require(cmd == {'tool': m['tools'][0]['path'], 'arguments': flags + ['-MD', '-MF', original_obj + '.d', '-c', str(old_root / rel), '-o', original_obj]}, 'Exact original compile argv')
        locks[obj] = sha(obj); locks[dep] = sha(dep)
        ordered_objects.append(original_obj)
    inf_rel = 'drivers/network/mt6630/' + NAME + '.inf'
    inf = cold / 'package/provider' / (NAME + '.inf')
    require(blob(compiled, inf_rel) == blob('HEAD', inf_rel) and checkout_matches(blob(compiled, inf_rel), sha(inf)), 'Exact compiled/current INF')
    text = decode_inf(inf.read_bytes())[0]
    versions = re.findall(r'^\s*DriverVer\s*=\s*([^\r\n]+)', text, re.I | re.M)
    require(versions == [m['driver_ver']], 'Payload INF version')
    require(re.search(r'^CatalogFile\s*=\s*' + re.escape(NAME) + r'\.cat\s*$', text, re.I | re.M), 'Catalog identity')
    pkg = inf.parent
    require({p.name for p in pkg.iterdir()} == {NAME + '.sys', NAME + '.inf'}, 'Unsigned package contains exactly SYS/INF')
    require(len(m['unsigned_outputs']) == 2 and {x['path'] for x in m['unsigned_outputs']} == {NAME + '.sys', NAME + '.inf'}, 'Two manifest payload leaves')
    for x in m['unsigned_outputs']:
        p = pkg / x['path']
        require(p.stat().st_size == x['bytes'] and sha(p) == x['sha256'], 'Unsigned payload hash/size')
        locks[p] = sha(p)
    sys = pkg / (NAME + '.sys')
    p = pe(sys.read_bytes())
    require(p['certificate'] == p['certificate_bytes'] == 0, 'Genuinely unsigned ARMNT SYS')
    args = ['/nologo', '/driver', '/subsystem:native,6.03', '/machine:arm', '/entry:DriverEntry', '/nodefaultlib', '/opt:ref', '/incremental:no'] + ordered_objects + [x['path'] for x in m['libraries']] + ['/out:' + str(old_out / 'package/provider' / (NAME + '.sys')), '/map:' + str(old_out / 'provider.map')]
    require(m['commands'][-1] == {'tool': m['tools'][1]['path'], 'arguments': args}, 'Original link order/argv')
    observed = cold / 'actual-dependencies.json'
    if observed.exists():
        d = read(observed)
        require(d['schema'] == 'mt6630-cold-actual-dependencies-v1' and d['source_commit'] == compiled and d['unit_count'] == len(d['units']) == 43, 'Optional dependency observation schema/epoch')
        require([{'source': u['source'], 'object': u['object']} for u in d['units']] == plan['objects'], 'Dependency observation TU order')
        for unit in d['units']:
            for dep in unit['dependencies']:
                path = PureWindowsPath(dep['path'])
                require(re.fullmatch('[0-9a-f]{64}', dep['sha256']) and dep['bytes'] > 0, 'Dependency observation hash/size')
                if path.is_relative_to(old_root) and not path.is_relative_to(old_root / 'build'):
                    rel = path.relative_to(old_root).as_posix()
                    require(blob(compiled, rel) == blob('HEAD', rel) and checkout_matches(blob(compiled, rel), dep['sha256']), 'Observed repository header/source changed: ' + rel)
        locks[observed] = sha(observed)
    return m, locks


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cold-directory', type=Path, required=True)
    ap.add_argument('--manifest-sha256', required=True)
    ap.add_argument('--compiled-source-commit', required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--thumbprint', required=True)
    ap.add_argument('--signtool', type=Path)
    ap.add_argument('--inf2cat', type=Path)
    ap.add_argument('--execute', action='store_true')
    a = ap.parse_args()
    thumb = normalize_thumbprint(a.thumbprint)
    cold = a.cold_directory.resolve(); out = a.output.resolve()
    require(not out.exists() and out.is_relative_to(ROOT) and not out.is_relative_to(cold) and not cold.is_relative_to(out), 'Fresh checkout-local output, separate from immutable cold inputs')
    recipe_commit = git('rev-parse', 'HEAD').decode().strip()
    require(not git('status', '--porcelain'), 'Clean committed signer source required')
    m, locks = validate(cold, a.manifest_sha256, a.compiled_source_commit)
    receipt = dict(schema='mt6630-public-cold-signing-v1', source_commit=m['source_commit'], compiled_source_commit=m['source_commit'], signing_recipe_commit=recipe_commit, original_unsigned_manifest_sha256=a.manifest_sha256, version=m['version'], driver_ver=m['driver_ver'], objects=m['objects'], flags=m['flags'], libraries=m['libraries'], compile_tools=m['tools'], compile_count=0, link_count=0, sign_count=0, signed=False, success=False, private_driver_baseline_required=False, hardware_verified=False, association_verified=False, steps={})
    receipt['selected_thumbprint'] = thumb
    receipt['certificate_store'] = 'CurrentUser/My'
    observed = cold / 'actual-dependencies.json'
    receipt['actual_dependency_observation_sha256'] = sha(observed) if observed.exists() else None
    receipt['dependency_observation_timing'] = 'after-compile; not a precompile environment proof'
    if not a.execute:
        print(json.dumps(receipt, indent=2)); return
    require(os.name == 'nt', 'Signing execution requires native Windows')
    require(a.signtool and a.inf2cat, 'Explicit existing signing tool paths required')
    pins = read(HERE / 'mt6630-provider1030-signing-pins.json')
    tools = {'signtool': a.signtool.resolve(), 'inf2cat': a.inf2cat.resolve()}
    for n, p in tools.items():
        require(sha(p) == pins[n], 'Existing signing tool SHA: ' + n); locks[p] = sha(p)
    out.mkdir(parents=True); (out / 'logs').mkdir(); pkg = out / 'package/provider'; pkg.mkdir(parents=True)
    receipt['signing_tools'] = {n: dict(path=str(p), sha256=sha(p)) for n, p in tools.items()}
    def save():
        (out / 'signing-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
    def recheck():
        require(git('rev-parse', 'HEAD').decode().strip() == recipe_commit and not git('status', '--porcelain'), 'Signing source changed')
        for p, digest in locks.items():
            require(sha(p) == digest, 'Immutable signing input changed: ' + str(p))
    def run(label, argv):
        require(label not in receipt['steps'], 'No automatic replay of signing stages')
        receipt['steps'][label] = dict(arguments=list(map(str, argv)), exit_code=None); save()
        env = dict(os.environ, TMP=str(out / 'logs'), TEMP=str(out / 'logs'))
        proc = subprocess.run(list(map(str, argv)), cwd=ROOT, env=env, capture_output=True, text=True, errors='replace')
        p = out / 'logs' / (label + '.log'); p.write_text(proc.stdout + proc.stderr)
        receipt['steps'][label].update(exit_code=proc.returncode, log_sha256=sha(p))
        receipt['sign_count'] = sum(k in ('sign-sys', 'sign-cat') and v['exit_code'] == 0 for k, v in receipt['steps'].items())
        save()
        require(proc.returncode == 0, 'Failed stage; preserve outputs, do not rerun: ' + label)
    try:
        save(); recheck()
        # Check only certificate metadata. Never import/export a key or handle a password.
        check = "$ErrorActionPreference='Stop';$c=Get-Item -LiteralPath 'Cert:\\CurrentUser\\My\\" + thumb + "';if(!$c.HasPrivateKey -or $c.NotBefore -gt (Get-Date) -or $c.NotAfter -lt (Get-Date)){throw 'Existing signing certificate unavailable'}"
        run('existing-store', ['powershell.exe', '-NoProfile', '-NonInteractive', '-Command', check])
        original = out / 'provenance'; original.mkdir()
        shutil.copyfile(cold / 'unsigned-manifest.json', original / 'unsigned-manifest.json')
        if observed.exists():
            shutil.copyfile(observed, original / observed.name)
        for ext in ('sys', 'inf'):
            shutil.copyfile(cold / 'package/provider' / (NAME + '.' + ext), pkg / (NAME + '.' + ext))
        sys = pkg / (NAME + '.sys'); cat = pkg / (NAME + '.cat'); inf = pkg / (NAME + '.inf')
        unsigned = sys.read_bytes()
        require(sha(original / 'unsigned-manifest.json') == a.manifest_sha256, 'Keep original manifest bytes')
        run('sign-sys', [tools['signtool'], 'sign', '/fd', 'SHA256', '/s', 'My', '/sha1', thumb, sys])
        unchanged_signed_payload(unsigned, sys.read_bytes()); recheck()
        run('catalog', [tools['inf2cat'], '/driver:' + str(pkg), '/os:6_3_ARM', '/uselocaltime'])
        run('sign-cat', [tools['signtool'], 'sign', '/fd', 'SHA256', '/s', 'My', '/sha1', thumb, cat])
        for ext, p in (('sys', sys), ('cat', cat)):
            run('verify-' + ext, [tools['signtool'], 'verify', '/pa', '/v', p])
        for ext, p in (('sys', sys), ('inf', inf)):
            run('membership-' + ext, [tools['signtool'], 'verify', '/pa', '/c', cat, p])
        paths = out / 'signature-paths.json'; paths.write_text(json.dumps([str(sys), str(cat)]))
        run('certificate-identity', ['powershell.exe', '-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', HERE / 'verify-public-signing-certificates.ps1', '-PathsJson', paths, '-ExpectedThumbprint', thumb, '-ReportJson', out / 'certificate-report.json'])
        recheck(); unchanged_signed_payload(unsigned, sys.read_bytes())
        require(sha(original / 'unsigned-manifest.json') == a.manifest_sha256, 'Original cold manifest remained byte-identical')
        if observed.exists():
            require(sha(original / observed.name) == locks[observed], 'Original dependency observation remained byte-identical')
        require(sha(inf) == locks[cold / 'package/provider' / inf.name], 'INF bytes must remain unchanged')
        require({p.name for p in pkg.iterdir()} == {NAME + '.' + ext for ext in ('sys', 'inf', 'cat')}, 'Signed package leaf set')
        receipt.update(success=True, signed=True, signer_thumbprint=thumb, non_signature_payload_unchanged=True, unsigned_SYS_sha256=hashlib.sha256(unsigned).hexdigest(), original_unsigned_manifest_preserved=True, files=[dict(path=p.name, bytes=p.stat().st_size, sha256=sha(p)) for p in sorted(pkg.iterdir())]); save()
        (pkg / 'signed-manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
    except Exception as e:
        receipt['error'] = str(e); raise
    finally:
        save()


if __name__ == '__main__':
    main()
