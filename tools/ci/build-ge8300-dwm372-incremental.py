#!/usr/bin/env python3
"""DWM370 -> DWM372 timing composition. Explicit opt-in; no native/UMD/PCO/SDK build."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PureWindowsPath
import re
import shutil
import subprocess
import zipfile
import concurrent.futures,threading,time
from ge8300_dwm372_dependencies import present_compatibility
from ge8300_compiler_cache import dependencies
ROOT = Path(__file__).resolve().parents[2]
HASH_CACHE = {}
HASH_LOCK = threading.RLock()
BASE = '30cb20f79ec3c5f71c056d5783fe6743e92bf2ce'

def require(ok, text):
    if not ok:
        raise ValueError(text)

def sha(path):
    path=Path(path).resolve();key=str(path).lower()
    with HASH_LOCK:
        if key not in HASH_CACHE:
            with path.open('rb') as stream:HASH_CACHE[key]=hashlib.file_digest(stream, 'sha256').hexdigest()
        return HASH_CACHE[key]

def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8-sig'))

def pin(path, digest):
    require(sha(path) == digest.lower(), 'Input pin mismatch: '+str(path))

def clean_source():
    require(not subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']), 'Clean source required')
    return subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()

def members(path):
    """Read real COFF archive bytes; never infer objects from source hashes."""
    data = Path(path).read_bytes()
    require(data[:8] == b'!<arch>\n', 'Not an archive')
    offset, table, rows = 8, b'', []
    while offset < len(data):
        header = data[offset:offset+60]
        require(len(header) == 60 and header[58:60] == b'`\n', 'Bad archive header')
        size = int(header[48:58]); name = header[:16].decode('ascii').strip()
        body = data[offset+60:offset+60+size]
        require(len(body) == size, 'Truncated archive')
        if name == '//':
            table = body
        elif name not in ('/', '/SYM64/'):
            if name.startswith('/'):
                start = int(name[1:]); require(start < len(table), 'Bad long name')
                end = start
                while end < len(table) and table[end] not in (0,10): end += 1
                name = table[start:end].decode('utf-8').rstrip('/')
            else:
                name = name.rstrip('/')
            name = PureWindowsPath(name).name
            require(name.endswith('.obj') and name not in [n for n,_ in rows], 'Ambiguous archive member')
            rows.append((name,body))
        offset += 60 + size + (size & 1)
    require(offset == len(data), 'Archive alignment invalid')
    return rows

def bound_link_inputs(spec, environment):
    # Bind the actual child environment used by the unchanged public helper,
    # not just an unrelated list of files with acceptable bytes.
    matches=[v for k,v in environment.items() if k.casefold()=='programfiles(x86)']
    require(len(matches)==1 and bool(matches[0]),'One unambiguous actual ProgramFiles(x86) kit mapping is required')
    kit=matches[0]
    km=Path(kit)/'Windows Kits/8.1/Lib/winv6.3/km/arm'
    expected={Path(re.sub(r'\\+',r'\\',r['path'])).name.lower():r for r in spec['link_inputs']}
    require(set(expected)=={'displib.lib','ntoskrnl.lib','hal.lib','libcntpr.lib','bufferoverflowfastfailk.lib'},'Exact five kernel libraries required')
    pin(ROOT/'drivers/gpu/mt8167/wddm-render/link-kmd-msvc-wdk81.ps1',spec['link_helper_sha256'])
    rows=[]
    for name,row in expected.items():
        actual=(km/name).resolve(strict=True);fixed=Path(re.sub(r'\\+',r'\\',row['path'])).resolve(strict=True)
        require(actual==fixed,'Actual link helper resolves an unpinned library: '+str(actual))
        pin(actual,row['sha256']);rows.append({'helper_selected_path':str(km/name),'resolved_path':str(actual),'sha256':sha(actual)})
    return rows

def validate(baseline, package, spec):
    for name, digest in spec['baseline_files'].items(): pin(baseline/name,digest)
    for row in spec['link_inputs']:pin(Path(re.sub(r'\\+',r'\\',row['path'])),row['sha256'])
    for name, digest in spec['package_files'].items(): pin(package/name,digest)
    for name, digest in spec['gpu_source_files'].items(): pin(ROOT/name,digest)
    actual = {p.relative_to(ROOT).as_posix() for p in (ROOT/'drivers/gpu/mt8167').rglob('*') if p.is_file() and p.suffix in ('.c','.h','.cxx','.hxx')}
    require(actual == set(spec['gpu_source_files']), 'GPU source closure added/dropped')
    for part in ('native','modules'):
        manifest = read(baseline/part/'build-manifest.json')
        require(manifest['source_commit'] == BASE and not manifest['source_dirty'], 'Wrong DWM370 source30cb baseline')
        for row in manifest['sources']:
            name = re.sub('/+','/',row['path'].replace('\\','/'))
            if name not in spec['timing_changed_files']: pin(ROOT/name,row['sha256'])
    for archive, expected in spec['archive_members'].items():
        require({n:hashlib.sha256(b).hexdigest() for n,b in members(baseline/archive)} == expected, 'Baseline archive member drift')
    return read(baseline/'modules/build-manifest.json')

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--enable-candidate',action='store_true'); p.add_argument('--validate-only',action='store_true')
    p.add_argument('--baseline-directory',type=Path,required=True);p.add_argument('--baseline-package',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--kernel-vc-tools-directory',type=Path,default=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133'))
    p.add_argument('--jobs',type=int,choices=range(1,5),default=2);p.add_argument('--plan-only',action='store_true')
    a=p.parse_args();require(a.enable_candidate,'Explicit opt-in required')
    commit=clean_source();spec=read(Path(__file__).with_name('ge8300-dwm372-incremental-pins.json'))
    baseline=a.baseline_directory.resolve();package=a.baseline_package.resolve();out=a.output.resolve()
    module=validate(baseline,package,spec)
    if a.validate_only:
        print(json.dumps(dict(validated=True,source_commit=commit,native_UMD_PCO_SDK_build_count=0,planned_CXX=10,planned_C=3,planned_SYS_links=2,executed=False)));return
    require(os.name=='nt','Windows execution only')
    require(not out.exists() and out.is_relative_to(ROOT) and not out.is_relative_to(baseline),'Fresh source-local output required')
    vc=a.kernel_vc_tools_directory.resolve();cl=vc/'bin/Hostx64/arm/cl.exe';link=vc/'bin/Hostx64/arm/link.exe'
    require(vc.name == '14.29.30133','Kernel VC14.29 required')
    pin(cl,spec['CL_sha256']);pin(link,spec['LINK_sha256'])
    powershell=Path(shutil.which('powershell') or '');pin(powershell,spec['signing_tools']['powershell.exe'])
    tools={r['name']:Path(re.sub(r'\\+',r'\\',r['path'])) for r in spec['baseline_tools']}
    for row in spec['baseline_tools']: pin(tools[row['name']],row['sha256'])
    native=baseline/'native/ge8300-native-owner-core.lib'
    for row in read(baseline/'native/build-manifest.json')['wdk_inputs']: pin(Path(re.sub(r'\\+',r'\\',row['path'])),row['sha256'])
    out.mkdir(parents=True);(out/'logs').mkdir();(out/'objects').mkdir();(out/'baseline-members').mkdir()
    env=dict(os.environ,VSLANG='1033',TEMP=str(out/'logs'),TMP=str(out/'logs'))
    receipt=dict(schema='dwm372-source30cb-incremental-v1',source_commit=commit,baseline_compiled_source_commit=BASE,whole_driver_fresh_build=False,native_compiled_source_commit=BASE,UMD_compiled_source_commit=BASE,native_recompiled=0,UMD_recompiled=0,PCO_recompiled=0,SDK_rebuilt=0,historical_include_closure_reconstructed=False,new_compiles=[],tools=[],commands=[],success=False,signed=False,hardware_verified=False,deployment_performed=False)
    def run(args,name):
        args=list(map(str,args));started=time.monotonic();q=subprocess.run(args,capture_output=True,text=True,errors='replace',env=env,cwd=ROOT)
        log=out/'logs'/name;log.write_text(q.stdout+q.stderr)
        receipt['commands'].append(dict(arguments=args,elapsed_seconds=time.monotonic()-started,returncode=q.returncode,log=log.relative_to(out).as_posix(),log_sha256=sha(log)))
        require(q.returncode==0,'Command failed; retained evidence: '+name);return q.stdout+q.stderr
    try:
        for name,tool in [('cl14.29',cl),('link14.29',link),('powershell',powershell),*tools.items()]: receipt['tools'].append(dict(name=name,path=str(tool),sha256=sha(tool)))
        run(['python','-B',ROOT/'tools/ci/verify-ge8300-fullsource.py'],'fullsource.log')
        receipt['actual_child_link_inputs_preflight']=bound_link_inputs(spec,env)
        compatibility=present_compatibility(ROOT,baseline,out/'compatibility',spec,sha,a.jobs,cl)
        receipt['present_compatibility_sha256']=sha(out/'compatibility/present-compatibility.json')
        receipt['plan_only']=a.plan_only
        if a.plan_only:
            receipt['success']=True;return
        replacement={}
        def compile_row(row):
            cpp=row['source'].endswith('.cxx')
            source=ROOT/'drivers/gpu/mt8167'/row['source'];obj=out/'objects'/row['object'];dep=obj.with_suffix('.d')
            args=[]
            for arg in row['arguments']:
                arg=re.sub('/+','/',arg.replace('\\','/'))
                if arg.startswith('/I') and arg.endswith('/drivers/gpu/mt8167/wddm-display'):arg='/I'+str(ROOT/'drivers/gpu/mt8167/wddm-display')
                args.append(arg)
            closure={str(source):sha(source)}
            if cpp:
                output=run([cl,*args,'/showIncludes',source,'/Fo'+str(obj)],obj.stem+'.log')
                headers=[Path(line.split('including file:',1)[1].strip()) for line in output.splitlines() if 'including file:' in line]
            else:
                run([tools['clang'],*args,'-MD','-MF',dep,'-MT','ge8300-unit','-c',source,'-o',obj],obj.stem+'.log')
                headers=[Path(name) for name in dependencies(dep)]
            require(headers,'Actual include trace required')
            for header in headers: closure[str(header.resolve(strict=True))]=sha(header)
            require(len(closure)>1,'Empty actual include closure')
            record=dict(source=source.relative_to(ROOT).as_posix(),source_sha256=sha(source),object=obj.name,object_sha256=sha(obj),tool_sha256=sha(cl if cpp else tools['clang']),arguments=args,actual_include_closure=closure,include_trace_kind='MSVC /showIncludes' if cpp else 'original clang -MD .d')
            if not cpp:record['original_dependency_sha256']=sha(dep)
            receipt['new_compiles'].append(record);replacement[obj.name]=obj
        selected=[r for r in [*module['unified_source_commands'],*spec['additional_changed_C']] if r['source'].endswith('.cxx') or r['source'] in ('wddm-render/display_adapter356.c','display-transport/transport356.c','wddm-render/miniport.c')]
        with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:list(pool.map(compile_row,selected))
        require(len(replacement)==13 and sum(x['source'].endswith('.cxx') for x in receipt['new_compiles'])==10,'Exactly10 C++ and3 C compiles required')
        archives={}
        for oldname,newname in [('ge8300-kmd-clear.lib','ge8300-kmd-clear.lib'),('ge8300-display-transport.lib','ge8300-display-transport.lib')]:
            inputs=[];rows=[];folder=out/'baseline-members'/oldname;folder.mkdir()
            for name,data in members(baseline/'modules'/oldname):
                old=folder/name;old.write_bytes(data);chosen=replacement.get(name,old);inputs.append(chosen)
                rows.append(dict(name=name,baseline_sha256=hashlib.sha256(data).hexdigest(),new_sha256=sha(chosen),recompiled=name in replacement))
            archive=out/newname;run([tools['llvm-lib'],'/machine:arm','/out:'+str(archive),*reversed(inputs)],newname+'.archive.log')
            require([n for n,b in members(archive)]==[r['name'] for r in rows],'Archive order changed')
            require({n:hashlib.sha256(b).hexdigest() for n,b in members(archive)}=={r['name']:r['new_sha256'] for r in rows},'New archive member drift')
            archives[newname]=dict(sha256=sha(archive),members=rows)
        receipt['archives']=archives
        receipt['actual_child_link_inputs']=bound_link_inputs(spec,env)
        for name,library in [('ge8300-render','ge8300-kmd-clear.lib'),('mt8167-display-transport','ge8300-display-transport.lib')]:
            run([powershell,'-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',ROOT/'drivers/gpu/mt8167/wddm-render/link-kmd-msvc-wdk81.ps1','-KernelLibrary',out/library,'-NativeLibrary',native,'-OutputFile',out/(name+'.sys'),'-MapFile',out/(name+'.map'),'-MetadataFile',out/(name+'-link.json'),'-MicrosoftLinker',link,'-UnifiedDisplay'],name+'.link.log')
            linked=read(out/(name+'-link.json'));require(linked['sha256']==spec['LINK_sha256'] and linked['version'].startswith('14.29.'),'Wrong actual linker')
            require(linked['output_sha256']==sha(out/(name+'.sys')),'Linked SYS hash drift')
            run([tools['llvm-readobj'],'--file-headers','--coff-imports',out/(name+'.sys')],name+'.pe.log')
        consumed=dict(HASH_CACHE)
        HASH_CACHE.clear()
        for path,digest in consumed.items():require(sha(Path(path))==digest,'Consumed input/output changed during batch: '+path)
        bound_link_inputs(spec,env)
        validate(baseline,package,spec);require(clean_source()==commit,'Source changed during build')
        for row in receipt['tools']:pin(Path(row['path']),row['sha256'])
        receipt['retained_native_include_trace_limits']={'compile_time_traces':38,'historical_unrecorded':29,'later_probes_are_not_historical':True}
        receipt['baseline_native_archive_sha256']=sha(native);receipt['baseline_package_manifest_sha256']=sha(package/'signed-manifest.json')
        receipt['success']=True
    except BaseException as error:
        receipt['error']=str(error);raise
    finally:
        receipt['outputs']={f.relative_to(out).as_posix():sha(f) for f in out.rglob('*') if f.is_file()}
        (out/'incremental-build-manifest.json').write_text(json.dumps(receipt,indent=2)+'\n')
        with zipfile.ZipFile(out.with_suffix('.zip'),'w',zipfile.ZIP_DEFLATED) as archive:
            for file in out.rglob('*'):
                if file.is_file():archive.write(file,file.relative_to(out))
    print('DWM372_13TU_2SYS_UNSIGNED_READY_NO_DEVICE_OPERATION')
if __name__=='__main__':main()
