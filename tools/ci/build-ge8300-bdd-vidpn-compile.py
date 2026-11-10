#!/usr/bin/env python3
"""固定VidPN修正BDD 1TUのMSVC14.29 ARM compileだけ。link/sign/配備なし。"""
import argparse,hashlib,json,os,re,shutil,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
RUNTIME='b63827d7c45e099d5844e2c694804a1df047ed10'
BASE_MANIFEST='87c6094416a8036baad6ff99e90f3c8edaff4e1eabb4582b7590177ce1cac9ff'
SOURCE='drivers/gpu/mt8167/wddm-display/bdd_dmm.cxx'
OLD_ROOT='C:/mt8167-panel-timing369-source'
def require(ok,text):
    if not ok:raise ValueError(text)
def sha(p):
    h=hashlib.sha256()
    with Path(p).open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()
def norm(p):return re.sub('/+','/',str(p).replace('\\','/'))
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--baseline-manifest',type=Path,required=True);ap.add_argument('--expected-source-commit',required=True);ap.add_argument('--output',type=Path,required=True)
    ap.add_argument('--vc-tools',type=Path,default=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133'))
    ap.add_argument('--wdk-include',type=Path,default=Path('C:/mt8167-public-fullsource323/wdk81/Include'))
    ap.add_argument('--um-include',type=Path,default=Path('C:/mt8167-minimal-service-host/cache/kit81/Include/um'))
    a=ap.parse_args();require(os.name=='nt','Windows native compile only')
    commit=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
    require(commit==a.expected_source_commit and not subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']),'Clean pinned source required')
    require(not subprocess.run(['git','-C',str(ROOT),'diff','--quiet',RUNTIME,'HEAD','--','drivers/gpu/mt8167']).returncode,'Runtime GPU source must equal approved b63827d7')
    require(sha(a.baseline_manifest)==BASE_MANIFEST,'Actual369 incremental manifest pin required')
    manifest=json.loads(a.baseline_manifest.read_text(encoding='utf-8-sig'));rows=[x for x in manifest['new_compiles'] if x['source']==SOURCE];require(len(rows)==1,'Exact BDD TU required');old=rows[0]
    require(manifest['success'] is True and old['actual_include_closure'],'Actual successful369 compile proof required')
    vc=a.vc_tools.resolve();require(vc.name=='14.29.30133','MSVC14.29.30133 only');cl=vc/'bin/Hostx64/arm/cl.exe';require(sha(cl)==old['tool_sha256']=='697a8f41ff7fcb71e7c6625f5517ef6e2f8ea146d1e351e078317ff92da0839c','Actual369 compiler byte required')
    out=a.output.resolve();require(out.is_relative_to(ROOT/'build') and not out.exists(),'Fresh checkout-local build output required');out.mkdir(parents=True);(out/'tmp').mkdir();(out/'include-witness').mkdir()
    maps=[(OLD_ROOT,ROOT),('C:/Program Files (x86)/Windows Kits/8.1/Include/km',a.wdk_include/'km'),('C:/Program Files (x86)/Windows Kits/8.1/Include/shared',a.wdk_include/'shared'),('C:/Program Files (x86)/Windows Kits/8.1/Include/um',a.um_include)]
    def rebound(p):
        p=norm(p)
        for before,after in maps:
            if p.lower()==before.lower() or p.lower().startswith(before.lower()+'/'):return Path(str(after)+p[len(before):])
        return Path(p)
    arguments=[('/I'+str(rebound(x[2:]))) if x.startswith('/I') else x for x in old['arguments']]
    require('/c' in arguments and '/kernel' in arguments and '/DA04_GE8300_UNIFIED_DISPLAY356=1' in arguments,'Original369 kernel/unified flags required')
    r=dict(schema='GE8300_BDD_VIDPN_ONE_TU_COMPILE_V1',source_commit=commit,runtime_source_commit=RUNTIME,baseline_manifest_sha256=BASE_MANIFEST,baseline_compiled_source_commit=manifest['source_commit'],source=SOURCE,source_sha256=sha(ROOT/SOURCE),compiler_path=str(cl),compiler_sha256=sha(cl),baseline_arguments=old['arguments'],arguments=arguments,explicit_path_rebindings=[dict(before=x,after=str(y)) for x,y in maps],compiled_TUs=0,link_count=0,sign_count=0,SDK_rebuilt=0,PCO_rebuilt=0,native_rebuilt=0,UMD_rebuilt=0,deployment_performed=False,hardware_verified=False,historical_include_closure_reconstructed=False,success=False)
    def save():(out/'compile-manifest.json').write_text(json.dumps(r,indent=2)+'\n')
    save()
    try:
        before=[]
        for p,h in old['actual_include_closure'].items():
            actual=rebound(p).resolve(strict=True);current=sha(actual)
            before.append(dict(baseline_path=p,actual_path=str(actual),baseline_sha256=h,actual_sha256=current,bytes_equal=current==h))
        r['baseline_closure_rebinding']=before
        r['tools']=[dict(path=str(p),sha256=sha(p)) for p in sorted((vc/'bin/Hostx64/arm').glob('*')) if p.is_file() and p.suffix.lower() in ('.exe','.dll')]
        obj=out/'display_bdd_dmm.obj';cmd=[str(cl),*arguments,'/showIncludes',str(ROOT/SOURCE),'/Fo'+str(obj)]
        env=dict(os.environ,VSLANG='1033',INCLUDE='',LIB='',LIBPATH='',TMP=str(out/'tmp'),TEMP=str(out/'tmp'))
        env['PATH']=str(cl.parent)+os.pathsep+env['PATH']
        q=subprocess.run(cmd,capture_output=True,text=True,errors='replace',cwd=ROOT,env=env)
        log=out/'compile.log';log.write_text(q.stdout+q.stderr);r.update(command=cmd,returncode=q.returncode,compile_log_sha256=sha(log));save()
        require(q.returncode==0,'BDD compile failed; stdout/receipt retained')
        headers={Path(line.split('including file:',1)[1].strip()).resolve(strict=True) for line in (q.stdout+q.stderr).splitlines() if 'including file:' in line}
        require(headers,'Actual MSVC include trace required');headers.add((ROOT/SOURCE).resolve())
        closure=[]
        for i,p in enumerate(sorted(headers)):
            dest=out/'include-witness'/('%03d-'%i+p.name);shutil.copyfile(p,dest);h=sha(p);require(sha(dest)==h,'Include witness copy mismatch')
            closure.append(dict(path=str(p),sha256=h,witness=dest.relative_to(out).as_posix()))
        for x in before:require(sha(x['actual_path'])==x['actual_sha256'],'Input changed during compile')
        for x in r['tools']:require(sha(x['path'])==x['sha256'],'Compiler tool changed during compile')
        actual={norm(x['path']).lower():x['sha256'] for x in closure};mapped={norm(x['actual_path']).lower() for x in before}
        r['include_delta']=[dict(baseline_path=x['baseline_path'],actual_path=x['actual_path'],baseline_sha256=x['baseline_sha256'],actual_sha256=actual.get(norm(x['actual_path']).lower()),status=('not-used' if norm(x['actual_path']).lower() not in actual else 'same-bytes' if actual[norm(x['actual_path']).lower()]==x['baseline_sha256'] else 'different-bytes')) for x in before]
        r['new_includes']=[x for x in closure if norm(x['path']).lower() not in mapped]
        require(int.from_bytes(obj.read_bytes()[:2],'little')==0x1c4,'ARMNT COFF object required')
        r.update(success=True,compiled_TUs=1,object_sha256=sha(obj),actual_include_closure=closure,actual_include_trace_kind='new MSVC14.29 /showIncludes; explicit current include roots',source_unchanged_during_compile=(sha(ROOT/SOURCE)==r['source_sha256']))
        require(r['source_unchanged_during_compile'],'Source changed during compile');save()
    except BaseException as e:r['error']=str(e);save();raise
    finally:
        with zipfile.ZipFile(out/'bdd-vidpn-compile-evidence.zip','w',zipfile.ZIP_DEFLATED) as z:
            for p in out.rglob('*'):
                if p.is_file() and p.suffix!='.zip':z.write(p,p.relative_to(out).as_posix())
if __name__=='__main__':main()
