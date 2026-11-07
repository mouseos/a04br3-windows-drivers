#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""実ARM compiler依存閉包をCRT初期化付きDLLへlinkする。OpenAdapter完成とは区別。"""
import argparse, hashlib, json, os, re, shutil, subprocess
from pathlib import Path

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for name in ('pco','nir','util','support'):
        ap.add_argument('--'+name+'-directory',type=Path,required=True)
    ap.add_argument('--vc-arm-libraries',type=Path,required=True)
    ap.add_argument('--sdk-arm-libraries',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args();here=Path(__file__).resolve().parent
    root=next(p for p in here.parents if (p/'windows-rt/AGENTS.md').is_file())
    commit=subprocess.check_output(['git','-C',str(here),'rev-parse','HEAD'],text=True).strip()
    if subprocess.check_output(['git','-C',str(here),'status','--porcelain'],text=True):
        raise SystemExit('link requires clean fixed source')
    out=a.output.resolve()
    if not out.is_relative_to(root):raise SystemExit('output outside project')
    inputs=[];manifests={}
    for name in ('pco','nir','util','support'):
        directory=getattr(a,name+'_directory').resolve()
        mpath=directory/'build-manifest.json';m=json.loads(mpath.read_text())
        if m['component']!=name or not m.get('archive_sha256') or any(u['exit_code'] for u in m['units']):
            raise SystemExit('incomplete compiler component: '+name)
        for u in m['units']:
            if sha(Path(u['object']))!=u['object_sha256']:raise SystemExit('object changed')
            source=Path(u['command'][u['command'].index('-c')+1])
            if sha(source)!=u['compiled_source_sha256']:raise SystemExit('compiled source changed')
            if not u.get('dependencies') or any(sha(Path(n))!=h for n,h in u['dependencies'].items()):
                raise SystemExit('compiler dependency changed')
            if any(f in u['command'] for f in ('-mfpu=none','/kernel','-msoft-float')):
                raise SystemExit('kernel/soft-float contract applied to user compiler')
        lib=directory/('ge8300-'+name+'-arm32.lib')
        if sha(lib)!=m['archive_sha256']:raise SystemExit('archive changed')
        inputs.append(dict(path=str(lib),sha256=sha(lib),role='compiler '+name))
        manifests[name]=dict(path=str(mpath),sha256=sha(mpath))
    for base,names in ((a.vc_arm_libraries,('libcmt.lib','libvcruntime.lib','libcpmt.lib')),
                       (a.sdk_arm_libraries,('libucrt.lib','kernel32.lib','synchronization.lib'))):
        for name in names:
            p=(base/name).resolve()
            inputs.append(dict(path=str(p),sha256=sha(p),role='Microsoft ARM static CRT / SDK'))
    out.mkdir(parents=True,exist_ok=False)
    env=os.environ.copy();env.update(TMPDIR=str(out),TEMP=str(out),TMP=str(out))
    cmd=['lld-link','/dll','/machine:arm','/subsystem:windows,6.03','/entry:_DllMainCRTStartup','/nodefaultlib',
         '/export:geo_pco_compile_tokenized','/export:geo_pco_compiler_init','/export:geo_pco_compiler_finish','/export:pco_ctx_create','/export:ralloc_free',
         *[i['path'] for i in inputs],'/out:'+str(out/'ge8300-pco.dll'),'/map:'+str(out/'ge8300-pco.map')]
    result=subprocess.run(cmd,env=env,capture_output=True,text=True)
    (out/'link.log').write_text(result.stdout+result.stderr)
    report=dict(source_commit=commit,source_dirty=False,inputs=inputs,component_manifests=manifests,command=cmd,exit_code=result.returncode,
                builder_sha256=sha(Path(__file__)),compiler_component_only=True,
                standard_d3d_connected=False,runtime_verified=False,crt_entry='_DllMainCRTStartup')
    if result.returncode==0:
        dll=out/'ge8300-pco.dll'
        reader=shutil.which('llvm-readobj') or shutil.which('llvm-readobj-18')
        if not reader:raise SystemExit('LLVM PE inspection tool missing')
        pe=subprocess.check_output([reader,'--file-headers','--coff-imports','--coff-exports',str(dll)],env=env,text=True)
        (out/'ge8300-pco.pe.txt').write_text(pe)
        if 'IMAGE_FILE_MACHINE_ARMNT' not in pe or 'OpenAdapter' in pe:raise SystemExit('unexpected component PE contract')
        imports=re.findall(r'^  Name: (.+[.]dll)$',pe,re.M|re.I)
        if not imports or any(n.lower() not in ('kernel32.dll','api-ms-win-core-synch-l1-2-0.dll') for n in imports):
            raise SystemExit('unexpected compiler CRT deployment dependency')
        report['dll_sha256']=sha(dll)
        report['import_modules']=imports
        report['user_mode_fp_enabled']=True
    for i in inputs:
        if sha(Path(i['path']))!=i['sha256']:raise SystemExit('input changed during link')
    (out/'link-manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    if result.returncode:raise SystemExit('real compiler DLL link failed: '+str(out/'link.log'))
    print('ARM32 compiler component DLL linked with real CRT; not a D3D UMD')
if __name__=='__main__':main()
