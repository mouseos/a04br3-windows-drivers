#!/usr/bin/env python3
"""377 archive + retained30cb UMD inputs; compile three TUs, link/sign wire378."""
import argparse,hashlib,importlib.util,json,os,re,shutil,subprocess,zipfile
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sp=importlib.util.spec_from_file_location('retained',HERE/'build-ge8300-gdi377-incremental.py');old=importlib.util.module_from_spec(sp);sp.loader.exec_module(old)
from ge8300_panel_signing_common import decode_inf,unchanged_signed_payload
from ge8300_compiler_cache import parse_make_dependencies
require,sha,read,pin,members=old.require,old.sha,old.read,old.pin,old.members
THUMB='BCBF6C456C6F0DD0818434763B189488FBF13570'
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for n in ('inputs','wdk-root','output','sign-tool','inf2cat'):ap.add_argument('--'+n,type=Path,required=True)
 ap.add_argument('--llvm-directory',type=Path,default=Path('C:/Program Files/LLVM/bin'))
 ap.add_argument('--kernel-linker',type=Path,default=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/link.exe'))
 ap.add_argument('--execute',action='store_true');ap.add_argument('--resume',action='store_true');a=ap.parse_args()
 spec=read(HERE/'ge8300-wire378-inputs.json');commit=old.old.reuse.clean_source();inputs=a.inputs.resolve();out=a.output.resolve();wdk=a.wdk_root.resolve()
 require(out.is_relative_to(ROOT) and not out.is_relative_to(inputs),'New checkout-local output required')
 def child(name):
  p=(inputs/name).resolve();require(p.is_relative_to(inputs),'Input path escape');return p
 for name,h in spec['files'].items():pin(child(name),h)
 inf=ROOT/'drivers/gpu/mt8167/wddm-render/ge8300-render.inf';inftext,encoding,bom=decode_inf(inf.read_bytes());require(re.search(r'^DriverVer=11/11/2026,0\.0\.3\.78\r?$',inftext,re.M),'Formal378 source INF required')
 baseline=read(child('package377/signed-manifest.json'));require(baseline['runtime_source_commit']=='956008469031c7d0a8872a821250214b43b905b2','Actual377 baseline')
 for r in baseline['outputs']:pin(child('package377/'+r['name']),r['sha256'])
 tools={n:a.llvm_directory/(n+'.exe') for n in ('clang','llvm-lib','llvm-readobj','lld-link')};tools['kernel-link']=a.kernel_linker;tools['signtool']=a.sign_tool;tools['inf2cat']=a.inf2cat;tools['powershell']=Path(shutil.which('powershell.exe') or 'C:/Windows/System32/WindowsPowerShell/v1.0/powershell.exe')
 for name,p in tools.items():pin(p,spec['tools'][name])
 km=wdk/'Lib/winv6.3/km/arm';klibs=[km/n for n in ('displib.lib','ntoskrnl.lib','hal.lib','libcntpr.lib','bufferoverflowfastfailk.lib')]
 for p in klibs:pin(p,spec['kernel_libraries'][p.name])
 compiler=read(child('compiler/link-manifest.json'));pair=read(child('compiler/pair-manifest.json'));libs=[]
 for r in compiler['inputs']:
  name=r['path'].replace('\\','/').split('/')[-1];p=child('compiler/'+name);pin(p,r['sha256']);libs.append(p)
 require(len(libs)==10 and compiler['source_commit']==pair['source_commit']=='30cb20f79ec3c5f71c056d5783fe6743e92bf2ce','Retained actual ten-library compiler/pair closure')
 pin(child('compiler/pco_pair.obj'),pair['object_sha256'])
 um=read(child('umd30cb/build-manifest.json'));require(um['compiler_manifest_sha256'].lower()==sha(child('compiler/link-manifest.json')) and um['pair_manifest_sha256'].lower()==sha(child('compiler/pair-manifest.json')),'Actual UMD/compiler/pair receipt binding')
 for r in um['outputs']:
  if r['name']=='ge8300-native-umd.lib':pin(child('umd30cb/'+r['name']),r['sha256'])
 plan=dict(schema='ge8300-wire378-three-tu-v1',source_commit=commit,candidate_version=378,driver_version='0.0.3.78',driver_date='11/11/2026',baseline377_manifest_sha256=sha(child('package377/signed-manifest.json')),retained_UMD_source_commit=compiler['source_commit'],compile_TUs=spec['sources'],SDK_rebuild=0,PCO_rebuild=0,native_owner_rebuild=0,SYS_links=1,UMD_links=1,hardware_verified=False)
 if not a.execute:print(json.dumps(dict(plan,executed=False),indent=2));return
 require(os.name=='nt','Windows build only');require(a.resume==out.exists(),'Explicit resume for existing output; never overwrite')
 out.mkdir(parents=True,exist_ok=True);(out/'logs').mkdir(exist_ok=True);(out/'objects').mkdir(exist_ok=True)
 state_path=out/'build-sign-receipt.json';state=read(state_path) if state_path.exists() else dict(plan,steps={},compiled=[],success=False)
 require(state['baseline377_manifest_sha256']==plan['baseline377_manifest_sha256'],'Resume input identity drift')
 if state['source_commit']!=commit:
  prior=state['source_commit'];subprocess.run(['git','-C',ROOT,'merge-base','--is-ancestor',prior,commit],check=True)
  changed=set(subprocess.check_output(['git','-C',ROOT,'diff','--name-only',prior,commit],text=True).splitlines())
  require(a.resume and changed and changed<={'tools/ci/build-sign-ge8300-wire378.py'},'Resume permits recipe-only correction; runtime/headers/INF must be byte-identical')
  for step in state['steps'].values():step.setdefault('source_commit',prior)
  state.setdefault('recipe_resume_history',[]).append(dict(previous=prior,current=commit,changed=sorted(changed),runtime_source_changed=False))
  state['source_commit']=commit
 def save():state_path.write_text(json.dumps(state,indent=2)+'\n')
 env=dict(os.environ,TMP=str(out/'logs'),TEMP=str(out/'logs'),VSLANG='1033')
 def run(name,args,outputs=()):
  args=list(map(str,args));prior=state['steps'].get(name)
  if prior:
   require(prior.get('exit_code')==0 and prior['arguments']==args,'Failed/uncertain step requires inspection, not automatic replay: '+name)
   for p,h in prior['outputs'].items():pin(Path(p),h)
   return (out/'logs'/name).read_text(errors='replace')
  log=out/'logs'/name;state['steps'][name]=dict(arguments=args,exit_code=None,outputs={});save()
  q=subprocess.run(args,cwd=ROOT,env=env,capture_output=True,text=True,errors='replace');log.write_text(q.stdout+q.stderr)
  state['steps'][name].update(exit_code=q.returncode,log_sha256=sha(log),outputs={str(p):sha(p) for p in outputs if p.exists()});save()
  require(q.returncode==0,'Failed step retained without replay: '+str(log));return q.stdout
 resource=Path(subprocess.check_output([tools['clang'],'-print-resource-dir'],text=True).strip())
 def relocate(arg):
  s=re.sub('/+','/',arg.replace('\\','/'))
  if s.startswith('-I'):
   path=s[2:];mark='/Windows Kits/8.1/'
   if mark in path:return '-I'+str(wdk/path.split(mark,1)[1])
   if '/LLVM/lib/clang/' in path:return '-I'+str(resource/'include')
  return arg
 try:
  replacement={}
  for row in spec['rows']:
   source=ROOT/row['source'];obj=out/'objects'/row['object'];dep=obj.with_suffix('.d');original=read(child(row['compile_receipt']));args=original['arguments'];cut=args.index('-c');flags=[relocate(x) for x in args[:cut]]
   run('compile-'+obj.stem+'.log',[tools['clang'],*flags,'-MD','-MF',dep,'-MT','wire378','-c',source,'-o',obj],[obj,dep])
   deptext=dep.read_text(encoding='utf-8-sig');require(deptext.startswith('wire378:'),'Exact wire378 dependency target required');deps=parse_make_dependencies('ge8300-unit:'+deptext[len('wire378:'):]);closure={str(Path(p).resolve()):sha(Path(p)) for p in deps};require(closure,'Actual dependency file missing')
   record=dict(source=row['source'],source_sha256=sha(source),object=row['object'],object_sha256=sha(obj),arguments=flags,actual_include_closure=closure,prior_compile_receipt_sha256=sha(child(row['compile_receipt'])),compile_recipe_commit=state['steps']['compile-'+obj.stem+'.log'].get('source_commit',commit))
   state['compiled']=[r for r in state['compiled'] if r['object']!=obj.name]+[record];save();replacement[obj.name]=obj
  def archive(name,baseline_name,replaced):
   before=members(child(baseline_name));folder=out/(name+'-members');folder.mkdir(exist_ok=True);objects=[];expected={}
   require(set(replaced)<={n for n,_ in before},'Replacement member absent')
   for n,data in before:
    p=folder/n
    if p.exists():require(p.read_bytes()==data,'Retained member drift')
    else:p.write_bytes(data)
    p=replaced.get(n,p);objects.append(p);expected[n]=sha(p)
   dest=out/name;run(name+'.log',[tools['llvm-lib'],'/machine:arm','/out:'+str(dest),*reversed(objects)],[dest]);actual=members(dest)
   require([n for n,_ in actual]==[n for n,_ in before] and {n:hashlib.sha256(b).hexdigest() for n,b in actual}==expected,'Archive order/hash drift')
   state.setdefault('archives',{})[name]=dict(baseline_sha256=sha(child(baseline_name)),replacement_members=sorted(replaced),members=expected);save();return dest
  kmd=archive('ge8300-kmd-clear.lib','baseline377/ge8300-kmd-clear.lib',{'query_adapter.obj':replacement['query_adapter.obj']})
  umd=archive('ge8300-native-umd.lib','umd30cb/ge8300-native-umd.lib',{'umd_frontend.obj':replacement['umd_frontend.obj']})
  native=child('baseline377/ge8300-native-owner-core.lib');sys=out/'ge8300-render.sys';dll=out/'ge8300-umd.dll'
  run('kernel-link.log',[tools['kernel-link'],'/largeaddressaware','/tsaware:no','/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/base:0x400000',kmd,native,*klibs,'/out:'+str(sys),'/map:'+str(out/'ge8300-render.map')],[sys])
  run('umd-link.log',[tools['lld-link'],'/dll','/machine:arm','/subsystem:windows,6.03','/entry:_DllMainCRTStartup','/nodefaultlib',child('umd30cb/umd_entry.obj'),replacement['umd_typed_entry.obj'],umd,child('compiler/pco_pair.obj'),*libs,'/out:'+str(dll),'/map:'+str(out/'ge8300-umd.map')],[dll])
  for name in ('ge8300-render.sys','ge8300-umd.dll'):run(name+'.pe.log',[tools['llvm-readobj'],'--file-headers','--coff-imports','--coff-exports',out/name])
  exports=run('baseline-umd.exports.log',[tools['llvm-readobj'],'--coff-exports',child('package377/ge8300-umd.dll')])
  names=lambda t:re.findall(r'Export \{.*?Name: ([^\r\n]+)',t,re.S)
  require(names(exports)==names((out/'logs/ge8300-umd.dll.pe.log').read_text()) and names(exports),'UMD export ABI drift; retained import library invalid')
  pkg=out/'package378'
  if not pkg.exists():
   pkg.mkdir()
   for r in baseline['outputs']:
    p=pkg/r['name'];p.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(child('package377/'+r['name']),p)
   for n in ('ge8300-render.sys','ge8300-umd.dll'):shutil.copyfile(out/n,pkg/n)
   oldinf=decode_inf((pkg/'ge8300-render.inf').read_bytes())[0].replace('\r\n','\n');newinf=inftext.replace('\r\n','\n');require(newinf==oldinf.replace('0.0.3.77','0.0.3.78'),'Only source DriverVer may differ')
   (pkg/'ge8300-render.inf').write_bytes(bom+newinf.replace('\n','\r\n').encode(encoding));(pkg/'ge8300-render.cat').unlink()
  for n in ('ge8300-render.sys','ge8300-umd.dll'):
   run(n+'.sign.log',[tools['signtool'],'sign','/fd','SHA256','/s','My','/sha1',THUMB,pkg/n],[pkg/n]);unchanged_signed_payload((out/n).read_bytes(),(pkg/n).read_bytes())
  retained=out/'retained-transport';require(not retained.exists(),'Interrupted catalog stage: preserve and inspect transport')
  (pkg/'display-transport').rename(retained)
  try:run('inf2cat.log',[tools['inf2cat'],'/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime'])
  finally:retained.rename(pkg/'display-transport')
  run('catalog.sign.log',[tools['signtool'],'sign','/fd','SHA256','/s','My','/sha1',THUMB,pkg/'ge8300-render.cat'],[pkg/'ge8300-render.cat'])
  signed=[pkg/n for n in ('ge8300-render.sys','ge8300-umd.dll','ge8300-render.cat','display-transport/mt8167-display-transport.sys','display-transport/mt8167-display-transport.cat')]
  for i,p in enumerate(signed):run('signature-'+str(i)+'.log',[tools['signtool'],'verify','/pa','/v',p])
  for folder,cat,ns in [('', 'ge8300-render.cat',('ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw')),('display-transport','mt8167-display-transport.cat',('mt8167-display-transport.inf','mt8167-display-transport.sys'))]:
   for n in ns:run(n+'.membership.log',[tools['signtool'],'verify','/pa','/c',pkg/folder/cat,pkg/folder/n])
  paths=out/'signature-paths.json';paths.write_text(json.dumps(list(map(str,signed))))
  run('certificate.log',[tools['powershell'],'-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',HERE/'verify-dwm-certificates.ps1','-PathsJson',paths,'-ExpectedThumbprint',THUMB,'-ReportJson',out/'certificate-report.json'])
  require(len(read(out/'certificate-report.json')['files'])==5,'Exact BCBF signature bindings required')
  changed={'ge8300-render.sys','ge8300-umd.dll','ge8300-render.inf','ge8300-render.cat'};actual={p.relative_to(pkg).as_posix():sha(p) for p in pkg.rglob('*') if p.is_file()}
  require(set(actual)=={r['name'] for r in baseline['outputs']},'Package member set drift')
  for r in baseline['outputs']:require((actual[r['name']]!=r['sha256'])==(r['name'] in changed),'Unexpected package change: '+r['name'])
  old.old.reuse.HASH_CACHE.clear()
  for name,h in spec['files'].items():pin(child(name),h)
  for name,p in tools.items():pin(p,spec['tools'][name])
  for p in klibs:pin(p,spec['kernel_libraries'][p.name])
  for r in state['compiled']:
   for name,h in r['actual_include_closure'].items():pin(Path(name),h)
  require(old.old.reuse.clean_source()==commit,'Source drift')
  state.update(success=True,signed=True,runtime_source_commit=commit,recipe_source_commit=commit,signer_thumbprint=THUMB,native_owner_archive_sha256=sha(native),outputs=[dict(name=n,sha256=h) for n,h in sorted(actual.items())])
  (pkg/'signed-manifest.json').write_text(json.dumps(state,indent=2)+'\n');print('WIRE378_THREE_TU_SIGNED_NOT_DEVICE_VERIFIED')
 finally:save()
if __name__=='__main__':main()
