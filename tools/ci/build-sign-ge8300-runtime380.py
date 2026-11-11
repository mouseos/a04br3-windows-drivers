#!/usr/bin/env python3
"""Incremental 380 over pinned 379/377 archives; inspection is the default.
No compiler, signer, SDK installer or device command runs without --execute.
"""
import argparse, hashlib, importlib.util, json, os, re, shutil, subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent; ROOT=HERE.parents[1]; GPU='drivers/gpu/mt8167/'
sp=importlib.util.spec_from_file_location('runtime377',HERE/'build-ge8300-gdi377-incremental.py')
old=importlib.util.module_from_spec(sp);sp.loader.exec_module(old)
from ge8300_panel_signing_common import unchanged_signed_payload,decode_inf
sp=importlib.util.spec_from_file_location('actual_dependencies',HERE/'plan-ge8300-incremental-runtime.py')
dep_parser=importlib.util.module_from_spec(sp);sp.loader.exec_module(dep_parser)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
def require(v,msg):
 if not v:raise ValueError(msg)
def git(*args):return subprocess.check_output(['git','-C',ROOT,*args])
def ref(root,r,external=False):
 require(isinstance(r,dict) and set(r)=={'path','bytes','sha256'} and re.fullmatch('[0-9a-f]{64}',r['sha256']),'Immutable input reference required')
 p=(root/r['path']).resolve(strict=True);require((p.is_relative_to(root) or (external and p.is_absolute())) and p.stat().st_size==r['bytes'] and sha(p)==r['sha256'],'Input hash/size drift: '+r['path']);return p

def runtime_plan(spec,rev):
 require(spec['integration_base']=='012acad30e76b65c3c2b5c4b1ec81f209f7354fb','Reviewed IA integration base only')
 paths=git('diff','--name-only',spec['integration_base'],rev,'--',GPU).decode().splitlines()
 runtime={x for x in paths if x.endswith(('.c','.cxx','.h'))}
 rows=spec['units'];by_source={x['source']:x for x in rows};require(len(rows)==len(by_source)==len({(x['archive'],x['object']) for x in rows}),'Unique source/object rows')
 headers={x for x in runtime if x.endswith('.h')};changed=runtime-headers
 require(changed<=set(by_source),'Changed TU without exact original compile contract: '+str(sorted(changed-set(by_source))))
 # A newly introduced private include can affect additional unchanged consumers.
 # Existing shared-header modifications require a broader admitted plan/cold build.
 for h in headers:
  require(subprocess.run(['git','-C',ROOT,'cat-file','-e',spec['integration_base']+':'+h],capture_output=True).returncode!=0,'Existing shared header changed; revise TU plan instead of retaining unknown consumers: '+h)
 affected=set(changed);pending={Path(h).name for h in headers};seen=set()
 while pending:
  name=pending.pop();require(name not in seen,'Ambiguous include traversal');seen.add(name)
  for p in (ROOT/GPU).rglob('*'):
   if p.suffix not in ('.c','.cxx','.h'):continue
   if re.search(r'^\s*#\s*include\s*[<"][^>"\n]*\b'+re.escape(name)+r'[>"]',p.read_text(errors='strict'),re.M):
    rel=p.relative_to(ROOT).as_posix()
    if p.suffix=='.h':
     if p.name not in seen:pending.add(p.name)
    else:affected.add(rel)
 require(affected<=set(by_source),'New private header reaches unplanned TUs; revise plan/cold build: '+str(sorted(affected-set(by_source))))
 selected=[x for x in rows if x['source'] in affected]
 return dict(runtime_changes=sorted(runtime),new_private_headers=sorted(headers),compile_units=selected,retained_object_bytes_pinned=True,historical_all_TU_include_equality_claimed=False,source_dependency_method='exact Git delta plus private-include consumer closure; selected C/CXX actual dependency scans required before codegen')

def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for n in ('workspace','inputs','output'):ap.add_argument('--'+n,type=Path,required=True)
 ap.add_argument('--execute',action='store_true');a=ap.parse_args();workspace=a.workspace.resolve(strict=True);spec=read(a.inputs.resolve(strict=True));rev=git('rev-parse','HEAD').decode().strip()
 require(spec['schema']=='GE8300_RUNTIME380_INCREMENTAL_V1','Exact380 schema')
 require(not git('status','--porcelain').strip(),'Clean committed source required')
 parsed={};inputs={}
 for kind,x in spec['archives'].items():
  require(kind in ('kmd','native'),'Known archive role');p=ref(workspace,x['file']);inputs[kind]=p;parsed[kind]=old.members(p)
  require([n for n,_ in parsed[kind]]==x['member_order'] and {n:hashlib.sha256(b).hexdigest() for n,b in parsed[kind]}==x['member_hashes'],'Baseline archive member/order drift')
 require(set(parsed)=={'kmd','native'} and len(parsed['kmd'])==24 and len(parsed['native'])==67,'379 KMD24/native67 archives only')
 for x in spec['provenance']:ref(workspace,x)
 package_ref=ref(workspace,spec['package_manifest']);package=package_ref.parent;baseline=read(package_ref)
 for row in baseline['outputs']:
  p=(package/row['name']).resolve(strict=True);require(p.is_relative_to(package) and sha(p)==row['sha256'] and p.stat().st_size==row['bytes'],'Baseline signed package drift')
 umd=ref(workspace,spec['signed_UMD']);sidecar=read(ref(workspace,spec['signed_UMD_provenance']))
 require(spec['UMD_compiled_source']=='012acad30e76b65c3c2b5c4b1ec81f209f7354fb' and umd.name=='ge8300-umd.dll','Signed FLOAT/UNORM DLL input only; no UMD compile')
 require(sidecar['success'] is True and sidecar['signed'] is True and sidecar['source_commit']==spec['UMD_compiled_source'] and sidecar['signed_DLL']['sha256']==sha(umd) and sidecar['non_signature_payload_unchanged'] is True,'Actual signed UNORM sidecar binding')
 plan=runtime_plan(spec,rev)
 for row in plan['compile_units']:
  require(row['archive'] in parsed and row['object'] in dict(parsed[row['archive']]),'Replacement not a unique archived member')
  require(row['tool'] in ('clang','cl') and isinstance(row['arguments'],list),'Explicit original tool/flags')
 pending=not(re.fullmatch('[0-9a-f]{40}',spec.get('source_commit') or '') and spec.get('main_source_reviewed') is True and spec.get('Bacon_source_commit') and spec.get('Hegel_source_commit'))
 result=dict(schema='GE8300_RUNTIME380_PLAN_V1',source_commit=rev,version='0.0.3.80',pending_source_lock=pending,plan=plan,UMD_recompiled=False,SDK_rebuilt=False,PCO_rebuilt=False,executed=False,signed=False,hardware_verified=False)
 if not a.execute:print(json.dumps(result,indent=2));return
 require(not pending and spec['source_commit']==rev,'Pending/wrong source lock; no compile/sign')
 require(os.name=='nt','Windows execution only')
 # Known driver-affecting implicit options must never alter the effective argv.
 require(not any(os.environ.get(k) for k in ('CL','_CL_','INCLUDE','LIB','CLANG_CONFIG_FILE_SYSTEM_DIR','CLANG_CONFIG_FILE_USER_DIR','CCC_OVERRIDE_OPTIONS','LIBPATH','LINK','_LINK_','CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','OBJC_INCLUDE_PATH')),'Implicit compiler/linker environment is set')
 out=a.output.resolve();require(out.is_relative_to(ROOT) and not out.exists(),'New checkout-local output required')
 for key in ('Hegel_source_commit','Bacon_source_commit'):git('merge-base','--is-ancestor',spec[key],rev)
 inf=ROOT/GPU/'wddm-render/ge8300-render.inf';text=decode_inf(inf.read_bytes())[0];require(len(re.findall(r'^DriverVer=[0-9/]+,0\.0\.3\.80\r?$',text,re.M))==1,'Integrated INF must explicitly declare0.0.3.80')
 tools={k:ref(workspace,v,external=True) for k,v in spec['tools'].items()};require(set(tools)=={'clang','cl','link','lib','readobj','sign','inf2cat'},'All original tools pinned')
 libs=[ref(workspace,x,external=True) for x in spec['kernel_libraries']];require([x.name for x in libs]==['displib.lib','ntoskrnl.lib','hal.lib','libcntpr.lib','bufferoverflowfastfailk.lib'],'Exact kernel link input order')
 for x in spec['external_inputs']:ref(workspace,x,external=True)
 external={str(ref(workspace,x,external=True)).lower():x['sha256'] for x in spec['external_inputs']}
 out.mkdir(parents=True);(out/'logs').mkdir();(out/'objects').mkdir()
 receipt=dict(result,executed=True,success=False,compiled_TUs=[],steps=[],retained_archives=spec['archives'],tools=spec['tools'],UMD=spec['signed_UMD'],implicit_environment_rejected=True)
 def save():(out/'build-sign-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 env=dict(os.environ,TMP=str(out/'logs'),TEMP=str(out/'logs'),VSLANG='1033')
 def run(label,args):
  record=dict(stage=label,arguments=list(map(str,args)),exit_code=None);receipt['steps'].append(record);save()
  with (out/'logs'/(label+'.log')).open('w') as log:
   q=subprocess.run(list(map(str,args)),cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT,text=True)
  record.update(exit_code=q.returncode,log_sha256=sha(out/'logs'/(label+'.log')));save();require(q.returncode==0,'Stage failed; preserve completed stages and never replay automatically: '+label)
  return (out/'logs'/(label+'.log')).read_text(errors='replace')
 def relocate(arg):
  for k,v in spec['argument_roots'].items():arg=arg.replace('${'+k+'}',str((workspace/v).resolve(strict=True)))
  arg=arg.replace('${SOURCE_ROOT}',str(ROOT));require('${' not in arg,'Unresolved original include/argument root');return arg
 def closure(paths,source):
  answer={}
  for p in [source,*paths]:
   p=p.resolve(strict=True);h=sha(p)
   if not p.is_relative_to(ROOT):require(external.get(str(p).lower())==h,'Unpinned actual external dependency: '+str(p))
   answer[str(p)]=h
  require(len(answer)>5,'Actual dependency closure absent');return answer
 try:
  prepared=[]
  # Complete every dependency admission before the first object compilation.
  for row in plan['compile_units']:
   source=ROOT/row['source'];flags=[relocate(x) for x in row['arguments']];name=row['object'];cpp=row['tool']=='cl'
   if cpp:
    flags=[f for f in flags if f.lower()!='/c'];output=run('deps-'+name,[tools['cl'],*flags,'/E','/showIncludes',source]);paths=[Path(x.strip()) for x in re.findall(r'^Note: including file:\s*(.+)$',output,re.M)]
   else:
    output=run('deps-'+name,[tools['clang'],'--no-default-config',*flags,'-M','-MT','__gpu_dependency_target',source]);paths=[Path(x) for x in dep_parser.make_dependencies(output)]
   deps=closure(paths,source);prepared.append((row,source,flags,deps))
  for row,source,flags,deps in prepared:
   obj=out/'objects'/row['object'];cpp=row['tool']=='cl'
   if cpp:
    output=run('compile-'+obj.name,[tools['cl'],*flags,'/c','/showIncludes','/Fo'+str(obj),source]);actual=closure([Path(x.strip()) for x in re.findall(r'^Note: including file:\s*(.+)$',output,re.M)],source)
   else:
    dep=obj.with_suffix('.d');run('compile-'+obj.name,[tools['clang'],'--no-default-config',*flags,'-MD','-MF',dep,'-MT','__gpu_dependency_target','-c',source,'-o',obj]);actual=closure([Path(x) for x in dep_parser.make_dependencies(dep.read_text())],source)
   require(actual==deps,'Actual compile dependency closure changed after admission')
   pe=run('ARMNT-'+obj.name,[tools['readobj'],'--file-headers',obj]);require('IMAGE_FILE_MACHINE_ARMNT' in pe,'New object is not ARMNT');receipt['compiled_TUs'].append(dict(row,object_sha256=sha(obj),actual_include_closure=actual));save()
  libraries={}
  for kind,archive in parsed.items():
   folder=out/('members-'+kind);folder.mkdir();objects=[];expected={};replace={x['object'] for x in plan['compile_units'] if x['archive']==kind}
   for name,data in archive:
    require(Path(name).name==name,'Unsafe archive member');p=folder/name;p.write_bytes(data);p=out/'objects'/name if name in replace else p;objects.append(p);expected[name]=sha(p)
   dest=out/inputs[kind].name;run('archive-'+kind,[tools['lib'],'/machine:arm','/out:'+str(dest),*reversed(objects)])
   after=old.members(dest);require([n for n,_ in after]==[n for n,_ in archive] and {n:hashlib.sha256(b).hexdigest() for n,b in after}==expected,'Retained/new member byte or order drift');libraries[kind]=dest
  unsigned=out/'ge8300-render.sys';run('link',[tools['link'],'/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000',libraries['kmd'],libraries['native'],*libs,'/out:'+str(unsigned),'/map:'+str(out/'ge8300-render.map')]);pe=run('PE',[tools['readobj'],'--file-headers','--coff-imports',unsigned]);require('IMAGE_FILE_MACHINE_ARMNT' in pe,'Linked SYS is not ARMNT')
  pkg=out/'package380';shutil.copytree(package,pkg);(pkg/'signed-manifest.json').unlink();(pkg/'ge8300-render.cat').unlink();shutil.copyfile(unsigned,pkg/unsigned.name);shutil.copyfile(inf,pkg/inf.name);shutil.copyfile(umd,pkg/umd.name)
  thumb=spec['signer_thumbprint'];require(thumb=='BCBF6C456C6F0DD0818434763B189488FBF13570','Existing development identity only')
  run('sign-SYS',[tools['sign'],'sign','/fd','SHA256','/s','My','/sha1',thumb,pkg/unsigned.name]);unchanged_signed_payload(unsigned.read_bytes(),(pkg/unsigned.name).read_bytes())
  transport=out/'retained-transport';(pkg/'display-transport').rename(transport)
  try:run('catalog',[tools['inf2cat'],'/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime'])
  finally:transport.rename(pkg/'display-transport')
  run('sign-CAT',[tools['sign'],'sign','/fd','SHA256','/s','My','/sha1',thumb,pkg/'ge8300-render.cat'])
  for name in ('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw'):run('member-'+name,[tools['sign'],'verify','/pa','/c',pkg/'ge8300-render.cat',pkg/name])
  signed=[pkg/x for x in ('ge8300-render.sys','ge8300-render.cat','ge8300-umd.dll','mt8167-dxgi-present356.exe','display-transport/mt8167-display-transport.sys','display-transport/mt8167-display-transport.cat')]
  paths=out/'signature-paths.json';paths.write_text(json.dumps(list(map(str,signed))));run('signer-identities',['powershell.exe','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',HERE/'verify-dwm-certificates.ps1','-PathsJson',paths,'-ExpectedThumbprint',thumb,'-ReportJson',out/'certificate-report.json'])
  actual={q.relative_to(pkg).as_posix():sha(q) for q in pkg.rglob('*') if q.is_file()};expected={x['name']:x['sha256'] for x in baseline['outputs']};require(set(actual)==set(expected),'Package member set drift')
  for name,h in actual.items():
   if name not in {'ge8300-render.inf','ge8300-render.sys','ge8300-render.cat','ge8300-umd.dll'}:require(h==expected[name],'Unrelated package byte drift: '+name)
  require(actual['ge8300-umd.dll']==sha(umd),'Signed UNORM UMD changed')
  for _,_,_,deps in prepared:
   for name,h in deps.items():require(sha(Path(name))==h,'Consumed input drift')
  require(git('rev-parse','HEAD').decode().strip()==rev and not git('status','--porcelain').strip(),'Source drift')
  for kind,x in spec['archives'].items():ref(workspace,x['file'])
  receipt.update(success=True,signed=True,signer_thumbprint=thumb,candidate_version=380,runtime_module_identity='retained miniport/query source; no literal BuildID inferred from package380',outputs=[dict(name=n,sha256=h,bytes=(pkg/n).stat().st_size) for n,h in sorted(actual.items())]);save();(pkg/'signed-manifest.json').write_text(json.dumps(receipt,indent=2)+'\n')
 finally:save()
if __name__=='__main__':main()
