#!/usr/bin/env python3
"""Reuse actual369 and the separately witnessed BDD TU; link/sign render371 only."""
import argparse, hashlib, importlib.util, json, os, re, shutil, subprocess, zipfile
from pathlib import Path
from ge8300_panel_signing_common import decode_inf, unchanged_signed_payload

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('timing_archive', Path(__file__).with_name('build-ge8300-panel-timing-incremental.py'))
archive = importlib.util.module_from_spec(spec)
spec.loader.exec_module(archive)
sha, read, require, pin = archive.sha, archive.read, archive.require, archive.pin
THUMB = 'BCBF6C456C6F0DD0818434763B189488FBF13570'
BASE_MANIFEST = '87c6094416a8036baad6ff99e90f3c8edaff4e1eabb4582b7590177ce1cac9ff'
BASE_PACKAGE = 'cdfb0064c25f49dceac669aaba0b7050c2d82f8d10b43c417f8ce6d2180cd08e'
BDD_RECEIPT = 'bed4f463cf75bfda21a4c999f542c40094dcb678c850ab90a5c31c180fe5adc8'

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--enable-candidate', action='store_true')
    for name in ('baseline-linked','baseline-package','native-library','bdd-evidence','output','kernel-library-root','kernel-input-lock','llvm-lib','llvm-readobj','microsoft-linker','signtool','inf2cat'):
        p.add_argument('--'+name, type=Path, required=True)
    p.add_argument('--kernel-input-lock-sha256', required=True)
    a = p.parse_args()
    require(a.enable_candidate and os.name == 'nt', 'Explicit Windows candidate required')
    require(not subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']), 'Clean checkout required')
    commit = subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'], text=True).strip()
    baseline, base, evidence, out = (v.resolve() for v in (a.baseline_linked,a.baseline_package,a.bdd_evidence,a.output))
    pin(baseline/'incremental-build-manifest.json', BASE_MANIFEST)
    pin(base/'signed-manifest.json', BASE_PACKAGE)
    pin(evidence/'compile-manifest.json', BDD_RECEIPT)
    b, signed, c = read(baseline/'incremental-build-manifest.json'), read(base/'signed-manifest.json'), read(evidence/'compile-manifest.json')
    require(b['success'] and signed['success'] and signed['signed'] and c['success'] and c['compiled_TUs'] == 1 and c['returncode'] == 0, 'Actual successful predecessor inputs required')
    require(c['baseline_manifest_sha256'] == BASE_MANIFEST and c['source'] == 'drivers/gpu/mt8167/wddm-display/bdd_dmm.cxx', 'Wrong BDD predecessor')
    pin(ROOT/c['source'], c['source_sha256'])
    pin(evidence/'display_bdd_dmm.obj', c['object_sha256'])
    subprocess.run(['git','-C',str(ROOT),'merge-base','--is-ancestor',c['source_commit'],commit], check=True)
    pins = read(Path(__file__).with_name('ge8300-panel-timing-incremental-pins.json'))
    for name, digest in pins['gpu_source_files'].items():
        if name != c['source']: pin(ROOT/name, digest)
    actual_sources = {v.relative_to(ROOT).as_posix() for v in (ROOT/'drivers/gpu/mt8167').rglob('*') if v.is_file() and v.suffix in ('.c','.h','.cxx','.hxx')}
    require(actual_sources == set(pins['gpu_source_files']), 'GPU closure added or removed')
    for row in signed['outputs']: pin(base/row['name'], row['sha256'])
    pin(a.native_library, b['baseline_native_archive_sha256'])
    for tool, key in ((a.llvm_lib,'llvm-lib'),(a.llvm_readobj,'llvm-readobj'),(a.microsoft_linker,'link14.29')):
        pin(tool, next(r['sha256'] for r in b['tools'] if r['name'] == key))
    for tool, key in ((a.signtool,'signtool.exe'),(a.inf2cat,'Inf2Cat.exe')): pin(tool, pins['signing_tools'][key])
    powershell = Path(shutil.which('powershell') or '')
    pin(powershell, pins['signing_tools']['powershell.exe'])
    km = a.kernel_library_root.resolve()
    pin(km/'displib.lib', read(baseline/'ge8300-render-link.json')['displib_sha256'])
    libraries = [km/(n+'.lib') for n in ('displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk')]
    pin(a.kernel_input_lock, a.kernel_input_lock_sha256)
    lock=read(a.kernel_input_lock)
    require(lock['schema']=='GE8300_VIDPN371_KERNEL_INPUT_LOCK_V1' and lock['baseline369_manifest_sha256']==BASE_MANIFEST, 'Wrong library input lock')
    require(lock['linker']['sha256']==sha(a.microsoft_linker), 'Locked linker identity mismatch')
    require(len(lock['libraries'])==5 and {r['name'] for r in lock['libraries']}=={v.name for v in libraries}, 'Incomplete library input lock')
    def check_libraries():
        pin(a.kernel_input_lock, a.kernel_input_lock_sha256)
        for row in lock['libraries']:
            f=km/row['name']; pin(f,row['sha256']); require(f.stat().st_size==row['bytes'], 'Library size mismatch')
    check_libraries()
    library_hashes = {v.name:sha(v) for v in libraries}
    original = baseline/'ge8300-kmd-clear.lib'
    pin(original, b['archives']['ge8300-kmd-clear.lib']['sha256'])
    members = archive.members(original)
    require(sum(n == 'display_bdd_dmm.obj' for n,_ in members) == 1, 'Unique actual BDD member required')
    require(not out.exists() and out.is_relative_to(ROOT), 'New checkout-local output required')
    out.mkdir(parents=True)
    (out/'evidence').mkdir(); (out/'members').mkdir()
    receipt = dict(schema='GE8300_VIDPN371_RELINK_SIGN_V1', recipe_source_commit=commit,
        runtime_source_commit=c['runtime_source_commit'], compiled_source_commit=c['source_commit'],
        driver_version='0.0.3.71', retained_transport_version='0.0.3.69',
        baseline_build_manifest_sha256=BASE_MANIFEST, baseline_package_manifest_sha256=BASE_PACKAGE,
        BDD_compile_receipt_sha256=BDD_RECEIPT, historical_compile_receipts_preserved=True,
        compile_count=0, link_count=0, SDK_rebuilt=False, PCO_rebuilt=False, UMD_rebuilt=False,
        hardware_verified=False, deployment_performed=False, success=False, signed=False,
        signer_thumbprint=THUMB, kernel_library_hashes=library_hashes,
        kernel_input_lock_sha256=a.kernel_input_lock_sha256, kernel_input_lock=lock,
        closure_scope=lock['closure_scope'], historical_other_four_library_equality_verified=False, receipts=[])
    env = os.environ.copy(); env.update(INCLUDE='',LIB='',LIBPATH='',TMP=str(out),TEMP=str(out),VSLANG='1033')
    def run(args, name):
        q = subprocess.run(list(map(str,args)), env=env, capture_output=True, text=True, errors='replace')
        log=out/'evidence'/name; log.write_text(q.stdout+q.stderr)
        receipt['receipts'].append(dict(arguments=list(map(str,args)),returncode=q.returncode,log=log.relative_to(out).as_posix(),sha256=sha(log)))
        require(q.returncode == 0, name+' failed')
    try:
        expected = {}
        for name, data in members:
            require(Path(name).name == name, 'Unsafe archive member name')
            if name == 'display_bdd_dmm.obj': data=(evidence/name).read_bytes()
            (out/'members'/name).write_bytes(data); expected[name]=hashlib.sha256(data).hexdigest()
        lib=out/'ge8300-kmd-clear.lib'
        # The pinned llvm-lib prepends inputs: reverse the command-line order
        # to retain the observed369 member order. Verify the actual result.
        run([a.llvm_lib,'/machine:arm','/out:'+str(lib),*[out/'members'/n for n in reversed(list(expected))]], 'archive.log')
        rebuilt=archive.members(lib)
        require([n for n,_ in rebuilt] == [n for n,_ in members], 'Archive member order changed')
        require({n:hashlib.sha256(v).hexdigest() for n,v in rebuilt} == expected, 'Archive member preservation failed')
        sys=out/'ge8300-render.sys'
        check_libraries()
        run([a.microsoft_linker,'/largeaddressaware','/tsaware:no','/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/base:0x400000',lib,a.native_library,*libraries,'/out:'+str(sys),'/map:'+str(out/'ge8300-render.map')], 'render.link.log')
        check_libraries()
        receipt['link_count']=1
        for path, name in ((base/'ge8300-render.sys','baseline.imports.log'),(sys,'candidate.imports.log')):
            run([a.llvm_readobj,'--coff-imports',path], name)
        def imports(name):
            return re.findall(r'^\s*(?:Name|Symbol):[^\r\n]*', (out/'evidence'/name).read_text(), re.M)
        require(imports('baseline.imports.log') and imports('baseline.imports.log') == imports('candidate.imports.log'), 'Kernel imports changed')
        receipt['kernel_imports_equal_actual369']=True
        package=out/'package371'; package.mkdir()
        for row in signed['outputs']:
            dest=package/row['name']; dest.parent.mkdir(parents=True,exist_ok=True); shutil.copyfile(base/row['name'],dest)
        dest=package/'ge8300-render.sys'; raw=sys.read_bytes(); dest.write_bytes(raw)
        run([a.signtool,'sign','/fd','SHA256','/s','My','/sha1',THUMB,dest], 'render.sign.log')
        unchanged_signed_payload(raw,dest.read_bytes())
        inf=package/'ge8300-render.inf'; text,encoding,bom=decode_inf(inf.read_bytes()); text=text.replace('\r\n','\n')
        require(text.count('DriverVer=11/10/2026,0.0.3.69') == 1, 'Exact369 INF expected')
        inf.write_bytes(bom+text.replace('DriverVer=11/10/2026,0.0.3.69','DriverVer=11/10/2026,0.0.3.71').replace('\n','\r\n').encode(encoding))
        (package/'ge8300-render.cat').unlink()
        transport=package/'display-transport'; saved=out/'retained-transport'; transport.rename(saved)
        try: run([a.inf2cat,'/driver:'+str(package),'/os:6_3_ARM','/uselocaltime'], 'render.inf2cat.log')
        finally: saved.rename(transport)
        cat=package/'ge8300-render.cat'
        run([a.signtool,'sign','/fd','SHA256','/s','My','/sha1',THUMB,cat], 'catalog.sign.log')
        for name in ('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw'):
            run([a.signtool,'verify','/pa','/v','/c',cat,package/name], name+'.membership.log')
        paths=out/'evidence/signature-paths.json'; paths.write_text(json.dumps([str(cat),str(dest)])+'\n')
        run([powershell,'-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',ROOT/'tools/ci/verify-panel-timing-certificates.ps1','-PathsJson',paths,'-ExpectedThumbprint',THUMB,'-ReportJson',out/'evidence/certificate-report.json'], 'certificate-identity.log')
        after={f.relative_to(package).as_posix():sha(f) for f in package.rglob('*') if f.is_file()}
        before={r['name']:r['sha256'] for r in signed['outputs']}
        changed={'ge8300-render.sys','ge8300-render.inf','ge8300-render.cat'}
        require(set(after) == set(before) and {n for n in after if after[n] != before[n]} == changed, 'Only render SYS/INF/CAT may change')
        for row in signed['outputs']: pin(base/row['name'], row['sha256'])
        require({v.name:sha(v) for v in libraries} == library_hashes, 'Kernel libraries changed')
        require(not subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']), 'Source changed during run')
        receipt.update(success=True,signed=True,outputs=[dict(name=n,sha256=h) for n,h in sorted(after.items())],archive_member_hashes=expected,
            unsigned_render_SYS_sha256=sha(sys),non_signature_payload_unchanged=True,new_display_source_commit=c['source_commit'],retained_UMD_FW_clients_transport=True)
        (package/'signed-manifest.json').write_text(json.dumps(receipt,indent=2)+'\n')
    except BaseException as error:
        receipt['error']=str(error); raise
    finally:
        (out/'relink-sign-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
        with zipfile.ZipFile(out.with_suffix('.zip'),'w',zipfile.ZIP_DEFLATED) as z:
            for f in out.rglob('*'):
                if f.is_file(): z.write(f,f.relative_to(out))
    print('VIDPN371_SIGNED_NOT_HARDWARE_VERIFIED')

if __name__ == '__main__': main()
