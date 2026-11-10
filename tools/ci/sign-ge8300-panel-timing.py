#!/usr/bin/env python3
"""Sign timed SYS pair; preserve signed GPU368 UMD/FW/client bytes."""
import argparse,hashlib,json,os,re,shutil,subprocess,zipfile
from pathlib import Path
from ge8300_panel_signing_common import require,decode_inf,unchanged_signed_payload
ROOT=Path(__file__).resolve().parents[2]
THUMB='BCBF6C456C6F0DD0818434763B189488FBF13570'
def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def pin(p,h):require(sha(p)==h.lower(),'Input hash mismatch: '+str(p))
def inf369(data,old):
    text,encoding,bom=decode_inf(data)
    logical=text.replace('\r\n','\n');require('\r' not in logical,'Unexpected INF line endings')
    matches=list(re.finditer(r'(?m)^DriverVer=([^\n]+)$',logical))
    require(len(matches)==1 and matches[0][1]==old,'Baseline INF identity mismatch')
    a,b=matches[0].span(1);new=logical[:a]+'11/10/2026,0.0.3.69'+logical[b:]
    return bom+new.replace('\n','\r\n').encode(encoding)
def main():
    p=argparse.ArgumentParser();p.add_argument('--enable-candidate',action='store_true');p.add_argument('--compiled-source-commit')
    p.add_argument('--baseline-package',type=Path,required=True);p.add_argument('--linked-directory',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--sign-tool',type=Path,required=True);p.add_argument('--inf2cat',type=Path,required=True)
    a=p.parse_args();require(a.enable_candidate and os.name=='nt','Explicit Windows signing opt-in required')
    require(not subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']),'Clean source required')
    commit=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
    spec=read(Path(__file__).with_name('ge8300-panel-timing-incremental-pins.json'));base=a.baseline_package.resolve();linked=a.linked_directory.resolve();out=a.output.resolve()
    for name,h in spec['package_files'].items():pin(base/name,h)
    build=read(linked/'incremental-build-manifest.json')
    compiled_source=a.compiled_source_commit or commit
    require(build['success'] and build['source_commit']==compiled_source,'Successful explicitly pinned compile source required')
    require(re.fullmatch('[0-9a-f]{40}',compiled_source),'Full compile commit required')
    # A signing-only recipe fix may differ from the real compile commit.
    # Verify current production bytes AND the originally recorded live closure;
    # never relabel the old object source or re-probe a replacement trace.
    for name,h in spec['gpu_source_files'].items():pin(ROOT/name,h)
    for row in build['new_compiles']:
        pin(ROOT/row['source'],row['source_sha256'])
        require(row['actual_include_closure'],'Original actual include trace required')
        for path,h in row['actual_include_closure'].items():pin(Path(path),h)
    for row in build['tools']:pin(Path(row['path']),row['sha256'])
    require(build['baseline_compiled_source_commit']==spec['baseline_source_commit'] and len(build['new_compiles'])==12,'Wrong incremental provenance')
    require(all(build[k]==0 for k in ('native_recompiled','UMD_recompiled','PCO_recompiled','SDK_rebuilt')),'Unexpected rebuild')
    for name,h in build['outputs'].items():pin(linked/name,h)
    powershell=Path(shutil.which('powershell') or '')
    for key,tool in [('signtool.exe',a.sign_tool),('Inf2Cat.exe',a.inf2cat),('powershell.exe',powershell)]:pin(tool,spec['signing_tools'][key])
    require(not out.exists() and out.is_relative_to(ROOT),'Fresh source-local signed output required')
    out.mkdir();(out/'evidence').mkdir();package=out/'package369';package.mkdir()
    receipt=dict(schema='panel-timing369-sign-only-v1',recipe_source_commit=commit,new_display_source_commit=compiled_source,explicit_compiled_source_commit=compiled_source,signing_recipe_only_change=(compiled_source!=commit),native_owner_source_commit=spec['baseline_source_commit'],umd_source_commit=spec['baseline_source_commit'],baseline_GPU368_manifest_sha256=sha(base/'signed-manifest.json'),input_build_manifest_sha256=sha(linked/'incremental-build-manifest.json'),compile_count=0,link_count=0,PCO_rebuilt=False,SDK_rebuilt=False,hardware_verified=False,deployment_performed=False,signed=False,success=False,signer_thumbprint=THUMB,receipts=[])
    def run(args,name):
        q=subprocess.run(list(map(str,args)),capture_output=True,text=True,errors='replace');log=out/'evidence'/name;log.write_text(q.stdout+q.stderr)
        receipt['receipts'].append(dict(arguments=list(map(str,args)),returncode=q.returncode,log=log.relative_to(out).as_posix(),sha256=sha(log)));require(q.returncode==0,'Signing command failed: '+name)
    try:
        for row in read(base/'signed-manifest.json')['outputs']:
            dest=package/row['name'];dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(base/row['name'],dest)
        transforms=[]
        for name in ('ge8300-render.sys','mt8167-display-transport.sys'):
            relative=name if name.startswith('ge8300') else 'display-transport/'+name;dest=package/relative
            raw=(linked/name).read_bytes();dest.write_bytes(raw)
            run([a.sign_tool,'sign','/fd','SHA256','/s','My','/sha1',THUMB,dest],name+'.sign.log')
            unchanged_signed_payload(raw,dest.read_bytes());transforms.append(dict(name=relative,unsigned_sha256=hashlib.sha256(raw).hexdigest(),signed_sha256=sha(dest),non_signature_payload_unchanged=True))
        for name,old in [('ge8300-render.inf','11/10/2026,0.0.3.68'),('display-transport/mt8167-display-transport.inf','11/09/2026,0.0.3.57')]:
            (package/name).write_bytes(inf369((base/name).read_bytes(),old))
        for cat in package.rglob('*.cat'):cat.unlink()
        retained=out/'transport-catalog-work';(package/'display-transport').rename(retained)
        try:run([a.inf2cat,'/driver:'+str(package),'/os:6_3_ARM','/uselocaltime'],'render.inf2cat.log')
        finally:retained.rename(package/'display-transport')
        run([a.inf2cat,'/driver:'+str(package/'display-transport'),'/os:6_3_ARM','/uselocaltime'],'transport.inf2cat.log')
        signed=[]
        for name in ('ge8300-render.cat','display-transport/mt8167-display-transport.cat'):
            run([a.sign_tool,'sign','/fd','SHA256','/s','My','/sha1',THUMB,package/name],Path(name).name+'.sign.log');signed.append(package/name)
        for name in ('ge8300-render.sys','display-transport/mt8167-display-transport.sys'):signed.append(package/name)
        for i,path in enumerate(signed):run([a.sign_tool,'verify','/pa','/v',path],str(i)+'.signature.log')
        for folder,cat,names in [('', 'ge8300-render.cat',('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw')),('display-transport','mt8167-display-transport.cat',('mt8167-display-transport.inf','mt8167-display-transport.sys'))]:
            for name in names:run([a.sign_tool,'verify','/pa','/v','/c',package/folder/cat,package/folder/name],name+'.catalog.log')
        paths=out/'evidence/signature-paths.json';paths.write_text(json.dumps(list(map(str,signed)))+'\n')
        run([powershell,'-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',ROOT/'tools/ci/verify-panel-timing-certificates.ps1','-PathsJson',paths,'-ExpectedThumbprint',THUMB,'-ReportJson',out/'evidence/certificate-report.json'],'certificate-identity.log')
        changed={'ge8300-render.sys','ge8300-render.inf','ge8300-render.cat','display-transport/mt8167-display-transport.sys','display-transport/mt8167-display-transport.inf','display-transport/mt8167-display-transport.cat'}
        before={r['name']:r['sha256'] for r in read(base/'signed-manifest.json')['outputs']}
        actual={f.relative_to(package).as_posix():sha(f) for f in package.rglob('*') if f.is_file()}
        require(set(actual)==set(before),'Final package file set drift')
        for name,h in actual.items():require((h!=before[name]) if name in changed else (h==before[name]),'Unexpected package delta: '+name)
        for name,h in spec['package_files'].items():pin(base/name,h)
        require(subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()==commit and not subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']),'Source changed during signing')
        receipt.update(signed=True,success=True,binary_signing_transforms=transforms,outputs=[dict(name=n,sha256=h) for n,h in sorted(actual.items())],retained_UMD_FW_clients=True,new_render_transport_SYS=True)
        (package/'signed-manifest.json').write_text(json.dumps(receipt,indent=2)+'\n')
    except BaseException as error:receipt['error']=str(error);raise
    finally:
        (out/'signing-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
        (out/'output-file-hashes.json').write_text(json.dumps({f.relative_to(out).as_posix():sha(f) for f in out.rglob('*') if f.is_file()},indent=2)+'\n')
        with zipfile.ZipFile(out.with_suffix('.zip'),'w',zipfile.ZIP_DEFLATED) as archive:
            for file in out.rglob('*'):
                if file.is_file():archive.write(file,file.relative_to(out))
    print('PANEL_TIMING369_SIGNED_RECOVERABLE_NOT_HARDWARE_SUCCESS')
if __name__=='__main__':main()
