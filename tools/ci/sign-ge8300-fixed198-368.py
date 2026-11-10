#!/usr/bin/env python3
"""Sign-only GPU368 fixed198 packaging (seven pinned outputs). No compile/link, installation, reset or runtime control."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
LOCK = Path(__file__).with_name('gpu368-sign-inputs.json')
CORE = ('ge8300-render.sys', 'ge8300-umd.dll')
CLIENTS = ('ge8300-d3d-clear.exe', 'ge8300-d3d-texture.exe', 'ge8300-d3d-texture-boundary.exe', 'ge8300-d3d-ui-texture.exe')
ADOPT = CORE + CLIENTS
TRANSPORT = tuple('display-transport/mt8167-display-transport.' + x for x in ('sys', 'inf', 'cat'))


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_pin(path, digest):
    data = Path(path).read_bytes()
    require(sha(data) == digest, 'Pinned input mismatch: ' + str(path))
    return data


def load_json(data):
    return json.loads(data.decode('utf-8-sig'))


def decode_inf(data):
    if data.startswith(b'\xff\xfe'):
        return data[2:].decode('utf-16-le'), 'utf-16-le', b'\xff\xfe'
    if data.startswith(b'\xfe\xff'):
        return data[2:].decode('utf-16-be'), 'utf-16-be', b'\xfe\xff'
    if data.startswith(b'\xef\xbb\xbf'):
        return data[3:].decode('utf-8'), 'utf-8', b'\xef\xbb\xbf'
    return data.decode('utf-8'), 'utf-8', b''


def inf368(data, pins):
    text, encoding, bom = decode_inf(data)
    logical = re.sub(r'\r+\n', '\n', text)
    require('\r' not in logical and '\x00' not in logical, 'Ambiguous INF line endings')
    hardware = pins['render_INF_hardware']
    require(all(token in logical for token in hardware), 'Actual hardware INF identity changed')
    lines = list(re.finditer(r'(?m)^DriverVer=([^\n]+)$', logical))
    require(len(lines) == 1, 'Exactly one DriverVer required')
    old = '11/10/2026,0.0.3.66'
    require(lines[0][1] == old, 'Baseline INF version mismatch')
    a, b = lines[0].span(1)
    changed = logical[:a] + pins['inf_driverver'] + logical[b:]
    result = bom + changed.replace('\n', '\r\n').encode(encoding)
    # Apart from DriverVer, the decoded logical lines remain identical.
    require(changed.replace(pins['inf_driverver'], old, 1) == logical, 'INF semantic drift')
    require(sha(result) == pins['render_INF368_sha256'], 'Normalized INF368 pin mismatch')
    return result


def pe(data):
    require(len(data) >= 64 and data[:2] == b'MZ', 'PE DOS header missing')
    offset = struct.unpack_from('<I', data, 60)[0]
    require(offset + 24 + 136 <= len(data) and data[offset:offset+4] == b'PE\0\0', 'PE header missing')
    require(struct.unpack_from('<H', data, offset+4)[0] == 0x1c4, 'ARMNT required')
    optional = offset + 24
    require(struct.unpack_from('<H', data, optional)[0] == 0x10b, 'ARM32 PE32 required')
    directory = optional + 96 + 4*8
    certificate, size = struct.unpack_from('<II', data, directory)
    return dict(checksum=optional+64, security_directory=directory, certificate=certificate, certificate_bytes=size)


def unchanged_signed_payload(unsigned, signed):
    before, after = pe(unsigned), pe(signed)
    require(before['certificate_bytes'] == 0 and after['certificate_bytes'] > 0, 'Unsigned to signed PE transition required')
    require(after['checksum'] == before['checksum'] and after['security_directory'] == before['security_directory'], 'PE header moved')
    require(len(signed) > len(unsigned) and sha(unsigned) != sha(signed), 'Signing must change file hash')
    c, n = after['certificate'], after['certificate_bytes']
    require(c >= len(unsigned) and c - len(unsigned) <= 7 and c % 8 == 0 and c+n <= len(signed), 'Unexpected certificate placement')
    a, b = bytearray(unsigned), bytearray(signed[:len(unsigned)])
    for start, length in ((before['checksum'], 4), (before['security_directory'], 8)):
        a[start:start+length] = b'\0' * length
        b[start:start+length] = b'\0' * length
    require(a == b and not any(signed[len(unsigned):c]) and not any(signed[c+n:]), 'Signing changed non-signature payload')


def validate_inputs(baseline, linked, review_path, proof_path, pins):
    old = load_json(read_pin(baseline/'signed-manifest.json', pins['baseline366_manifest_sha256']))
    require(old['schema'] == pins['baseline366_schema'], 'Baseline366 schema mismatch')
    require({x['name']:x['sha256'] for x in old['outputs']} == pins['baseline366_outputs'], 'Baseline output set drift')
    files = {n:read_pin(baseline/n, h) for n,h in pins['baseline366_outputs'].items()}
    artifacts = {n:read_pin(linked/n,h) for n,h in pins['artifact_files'].items()}
    metadata = load_json(artifacts['source-build-manifest.json'])
    require(metadata['schema'] == 'single-checkout-source-build327-v1', 'Source metadata schema')
    require(metadata['source_commit'] == pins['compiled_source_commit'] and metadata['source_dirty'] is False, 'Compile source mismatch')
    require(metadata['compiler_compiled_source_commit'] == pins['compiled_source_commit'], 'Compiler provenance mismatch')
    require(metadata['driver_signed'] is False and metadata['hardware_verified'] is False and metadata['DWM_verified'] is False, 'Preserve unsigned/non-hardware provenance')
    require({x['name']:x['sha256'].lower() for x in metadata['outputs']} == pins['unsigned_linked_outputs'], 'Seven output binding mismatch')
    for key, child in [('native_build_manifest_sha256','native/build-manifest.json'),('KMD_build_manifest_sha256','modules/build-manifest.json'),('UMD_build_manifest_sha256','umd/build-manifest.json'),('kernel_link_manifest_sha256','kernel-link.json'),('transport_link_manifest_sha256','transport-link.json'),('compiler_cache_archive_receipt_sha256','compiler375-cache.zip.receipt.json'),('fullsource_origins_sha256','source-origins.json')]:
        require(metadata[key].lower() == sha(artifacts[child]), 'Child manifest binding: '+key)
    review = load_json(read_pin(review_path,pins['main_review_sha256']))
    require(review['source_commit'] == pins['compiled_source_commit'] and review['source_dirty'] is False and review['hardware_verified'] is False, 'Main review provenance mismatch')
    require({x['name']:x['sha256'] for x in review['outputs']} == pins['unsigned_linked_outputs'], 'Main seven output mismatch')
    proof = load_json(read_pin(proof_path,pins['import_proof_sha256']))
    for n,row in proof.items():
        require(row['current_SHA256'] == pins['unsigned_linked_outputs'][n] and row['new_imports'] == [] and row['imports'] == row['baseline_imports'], 'Import comparison mismatch: '+n)
    new = {}
    for n,h in pins['unsigned_linked_outputs'].items():
        # Original VM wrapper stores UMD/client outputs in umd/; recovered
        # root aliases are accepted only when they match the identical pin.
        path = linked/n if (linked/n).is_file() else linked/'umd'/n
        data = read_pin(path,h)
        require(pe(data)['certificate_bytes'] == 0, 'Unsigned ARM32 PE required: '+n)
        if n in ADOPT:
            new[n] = data
    identity = pins['module_identity']
    require(all(x.encode()+b'\0' in new[CORE[0]] for x in (identity['runtime_build_id'],identity['runtime_source_base'])), 'Legacy runtime identity mismatch')
    read_pin(ROOT/identity['source'],identity['source_sha256'])
    for n,h in {**pins['transport_policy']['actual_local_compile_dependencies'], **pins['transport_policy']['unchanged_secondary_linked_source']}.items():
        read_pin(ROOT/n,h)
    inf368(files['ge8300-render.inf'],pins)
    return files,new,metadata,artifacts['source-build-manifest.json']


def stage(out, files, new, pins):
    package = out/'package368'
    package.mkdir()
    for name, data in files.items():
        if name == 'ge8300-render.cat':
            continue  # Never carry the old render CAT into the signing output.
        dst = package/name
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_bytes(data)
    for name in ADOPT:
        (package/name).write_bytes(new[name])
    (package/'ge8300-render.inf').write_bytes(inf368(files['ge8300-render.inf'], pins))
    return package


def run_tool(command, log):
    result = subprocess.run([str(x) for x in command], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    # No certificate email addresses are needed in signature diagnostics.
    text = re.sub(rb'[\w.+-]+@[\w.-]+\.[A-Za-z]{2,}', b'<email-redacted>', result.stdout)
    log.write_bytes(text)
    require(result.returncode == 0, 'Signing/verification failed: ' + log.name)
    return dict(command=[str(x) for x in command], exit_code=result.returncode, log=log.name, log_sha256=sha(text))


def make_manifest(pins, metadata, recipe_commit, outputs, transforms, verification):
    return dict(schema='GPU368_SIGN_ONLY_FIXED198_V1', source_commit=pins['compiled_source_commit'],
        package_source_commit=pins['compiled_source_commit'], recipe_source_commit=recipe_commit,
        kernel_binary_source_commit=pins['compiled_source_commit'], native_owner_source_commit=pins['compiled_source_commit'],
        umd_source_commit=pins['compiled_source_commit'], compiler_source_commit=metadata['compiler_compiled_source_commit'],
        input_source_build_manifest_sha256=pins['artifact_files']['source-build-manifest.json'],
        input_metadata_pins=pins['artifact_files'], main_input_review_sha256=pins['main_review_sha256'],
        import_comparison_sha256=pins['import_proof_sha256'], import_proof_is_saved_evidence_not_live_test=True,
        inf_driverver=pins['inf_driverver'], module_identity=dict(pins['module_identity'],inherited=True,observed_live=False),
        transport_policy=pins['transport_policy'], transport_source_commit=pins['transport_policy']['actual_transport_compile_source'],
        retained_baseline_files={n:h for n,h in pins['baseline366_outputs'].items() if n not in set(ADOPT)|{'ge8300-render.inf','ge8300-render.cat'}},
        shared_surface_contract=metadata['shared_surface_contract'], RT_SRV_profile=metadata['RT_SRV_profile'],
        blend_implemented=False, DWM_verified=False, hardware_verified=False, standard_profile_qualified=False,
        compile_invocations_this_signing_step=0, link_invocations_this_signing_step=0, SDK_rebuilt=0, PCO_rebuilt=0,
        service_modified=False, deployment_performed=False, signed=True, signer_thumbprint=pins['signer_thumbprint'],
        binary_signing_transforms=transforms, verification=verification, outputs=outputs,
        main_independent_artifact_review_performed=False)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline-package', type=Path, required=True)
    p.add_argument('--linked-directory', type=Path, required=True)
    p.add_argument('--main-review', type=Path, required=True)
    p.add_argument('--import-proof', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--sign-tool', type=Path)
    p.add_argument('--inf2cat', type=Path)
    p.add_argument('--powershell', default='powershell.exe')
    p.add_argument('--sign', action='store_true')
    a = p.parse_args()
    pins = load_json(LOCK.read_bytes())
    out = a.output.resolve()
    require(out.is_relative_to(ROOT/'build') or out.is_relative_to(ROOT/'out'), 'New checkout-local build/out directory required')
    require(not out.exists(), 'Output must not exist; retain earlier plan in its own directory')
    require(not subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']), 'Clean committed source required')
    commit = subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'], text=True).strip()
    files, new, metadata, metadata_raw = validate_inputs(a.baseline_package.resolve(), a.linked_directory.resolve(), a.main_review, a.import_proof, pins)
    if a.sign:
        require(os.name == 'nt', 'Actual signing is main-only on Windows')
        require(a.sign_tool and a.sign_tool.is_file() and a.sign_tool.name.lower() == 'signtool.exe', 'Existing SignTool required')
        require(a.inf2cat and a.inf2cat.is_file() and a.inf2cat.name.lower() == 'inf2cat.exe', 'Existing Inf2Cat required')
        powershell = shutil.which(a.powershell)
        require(powershell, 'Existing Windows PowerShell required for certificate identity checks')
    out.mkdir(parents=True)
    evidence = out/'evidence'
    evidence.mkdir()
    (evidence/'fixed198-input-manifest.json').write_bytes(metadata_raw)
    (evidence/'main-input-review.json').write_bytes(read_pin(a.main_review, pins['main_review_sha256']))
    (evidence/'main-import-proof.json').write_bytes(read_pin(a.import_proof,pins['import_proof_sha256']))
    plan = dict(schema='GPU368_SIGN_ONLY_PLAN_V1', package_source_commit=pins['compiled_source_commit'], recipe_source_commit=commit,
        source_build_manifest_sha256=pins['artifact_files']['source-build-manifest.json'], baseline366_manifest_sha256=pins['baseline366_manifest_sha256'],
        unsigned_inputs={k: sha(v) for k,v in new.items()}, normalized_INF_sha256=pins['render_INF368_sha256'],
        runtime_build_id=pins['module_identity']['runtime_build_id'], module_identity=pins['module_identity'],
        all_seven_unsigned_inputs=pins['unsigned_linked_outputs'], transport_policy=pins['transport_policy'], main_review_sha256=pins['main_review_sha256'],
        compile_invocations=0, link_invocations=0, signed=False, deployment_performed=False, cold_conditions_changed=False)
    (out/'signing-plan.json').write_text(json.dumps(plan, indent=2)+'\n')
    if not a.sign:
        print('GPU368_PINNED_PLAN_ONLY_NO_SIGNING_COMPILE_LINK_OR_DEVICE_OPERATION')
        return
    package = stage(out, files, new, pins)
    receipts = []
    def tool(args, name):
        row = run_tool(args, evidence/name)
        receipts.append(row)
    thumb = pins['signer_thumbprint']
    transforms = []
    signed_paths = []
    for name in ADOPT:
        path = package/name
        tool([a.sign_tool,'sign','/fd','SHA256','/s','My','/sha1',thumb,path], name+'.sign.log')
        data = path.read_bytes()
        unchanged_signed_payload(new[name], data)
        transforms.append(dict(name=name, input_state='unsigned-linked', input_sha256=sha(new[name]), output_state='signed', signed_sha256=sha(data), non_signature_payload_unchanged=True))
        signed_paths.append(path)
    # Keep the frozen transport CAT outside the render Inf2Cat scan. It is
    # restored verbatim; this is not selection/adoption of linked transport.
    retained = out/'retained-display-transport'
    (package/'display-transport').rename(retained)
    try:
        tool([a.inf2cat,'/driver:'+str(package),'/os:6_3_ARM','/uselocaltime'], 'render.catalog.log')
    finally:
        retained.rename(package/'display-transport')
    cat = package/'ge8300-render.cat'
    require(cat.is_file() and sha(cat.read_bytes()) != pins['baseline366_outputs']['ge8300-render.cat'], 'New render CAT required')
    tool([a.sign_tool,'sign','/fd','SHA256','/s','My','/sha1',thumb,cat], 'render.cat-sign.log')
    signed_paths.append(cat)
    membership = {}
    for name in (*CORE, 'ge8300-render.inf', 'ge8300-mips.fw'):
        tool([a.sign_tool,'verify','/pa','/v','/c',cat,package/name], name+'.catalog.log')
        membership[name] = 'verified-new-render-CAT'
    oldcat = package/TRANSPORT[2]
    for name in TRANSPORT[:2]:
        tool([a.sign_tool,'verify','/pa','/v','/c',oldcat,package/name], Path(name).name+'.baseline-catalog.log')
        membership[name] = 'verified-preserved-transport-CAT'
    signed_paths += [oldcat, package/TRANSPORT[0]]
    for index, path in enumerate(signed_paths):
        tool([a.sign_tool,'verify','/pa','/v',path], str(index)+'.signature.log')
    paths_json = evidence/'signature-paths.json'
    paths_json.write_text(json.dumps([str(p.resolve()) for p in signed_paths])+'\n')
    signature_helper = Path(__file__).with_name('verify-gpu368-certificates.ps1')
    tool([powershell,'-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',signature_helper,
          '-PathsJson',paths_json,'-ExpectedThumbprint',thumb,'-ReportJson',evidence/'certificate-report.json'], 'certificate-identity.log')
    # Validate every final filename/hash. Adopt six same-source binaries;
    # preserve transport/FW/auxiliary bytes and add the fourth client.
    actual_names = {f.relative_to(package).as_posix() for f in package.rglob('*') if f.is_file()}
    require(actual_names == set(pins['baseline366_outputs']) | set(ADOPT), 'Final package output set drift')
    outputs = []
    changed = set(ADOPT) | {'ge8300-render.inf','ge8300-render.cat'}
    for name in sorted(actual_names):
        data = (package/name).read_bytes()
        if name not in changed:
            require(sha(data) == pins['baseline366_outputs'][name], 'Retained baseline output changed: '+name)
        outputs.append(dict(name=name,sha256=sha(data)))
    require((package/'ge8300-render.inf').read_bytes() == inf368(files['ge8300-render.inf'], pins), 'Final INF drift')
    require(all(x.encode()+b'\0' in (package/CORE[0]).read_bytes() for x in (pins['module_identity']['runtime_build_id'],pins['module_identity']['runtime_source_base'])), 'Runtime identity changed while signing')
    coverage = {name: membership.get(name, 'independent-PE-signature; not-INF-CAT-member' if name in CLIENTS else 'CAT-signature-verified' if name.endswith('.cat') else 'retained-baseline-hash; auxiliary-not-listed-in-INF') for name in sorted(actual_names)}
    verification = dict(all_output_hashes_checked=True, catalog_coverage=coverage, signature_receipts=receipts,
        signer_identity_report_sha256=sha((evidence/'certificate-report.json').read_bytes()),
        tools=[dict(name=p.name,sha256=sha(p.read_bytes())) for p in (a.sign_tool,a.inf2cat,Path(powershell))])
    gm = make_manifest(pins, metadata, commit, outputs, transforms, verification)
    manifest = package/'signed-manifest.json'
    manifest.write_text(json.dumps(gm, indent=2)+'\n')
    require(not subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']), 'Source changed during signing')
    require(subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'], text=True).strip() == commit, 'Recipe HEAD changed')
    for name in ADOPT:
        path = a.linked_directory/name if (a.linked_directory/name).is_file() else a.linked_directory/'umd'/name
        read_pin(path, pins['unsigned_linked_outputs'][name])
    validate_inputs(a.baseline_package.resolve(),a.linked_directory.resolve(),a.main_review,a.import_proof,pins)
    hashes = {f.relative_to(out).as_posix(): sha(f.read_bytes()) for f in sorted(out.rglob('*')) if f.is_file()}
    (out/'output-file-hashes.json').write_text(json.dumps(hashes,indent=2)+'\n')
    print(json.dumps(dict(recipe_signed_package=True, package_source_commit=gm['package_source_commit'], recipe_source_commit=commit,
        manifest_sha256=sha(manifest.read_bytes()), SYS_sha256=sha((package/CORE[0]).read_bytes()), UMD_sha256=sha((package/CORE[1]).read_bytes()),
        main_independent_review=False, hardware_verified=False),separators=(',',':')))


if __name__ == '__main__':
    main()
