#!/usr/bin/env python3
"""376 retained archives + exact two C TUs -> one unsigned377 render link."""
import argparse, concurrent.futures, datetime, hashlib, importlib.util, json, os, re, shutil, subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('retained376',HERE/'build-ge8300-gdi376-incremental.py')
old=importlib.util.module_from_spec(sp);sp.loader.exec_module(old)
ROOT=HERE.parents[1]
require,sha,read,pin,members=old.require,old.sha,old.read,old.pin,old.members

def child(base,name):
 p=(base/name.replace('\\','/')).resolve();require(p.is_relative_to(base.resolve()),'Input output-path escape');return p

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--enable-candidate',action='store_true');ap.add_argument('--inputs',type=Path,required=True);ap.add_argument('--baseline376',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--plan-only',action='store_true');ap.add_argument('--llvm-directory',type=Path,default=Path('C:/Program Files/LLVM/bin'));ap.add_argument('--vc-directory',type=Path,default=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133'));ap.add_argument('--jobs',type=int,choices=(1,2),default=2);a=ap.parse_args()
 from ge8300_panel_signing_common import decode_inf
 inftext=decode_inf((ROOT/'drivers/gpu/mt8167/wddm-render/ge8300-render.inf').read_bytes())[0]
 dates=re.findall(r'^DriverVer=(11/(?:10|11)/2026),0\.0\.3\.77\r?$',inftext,re.M)
 require(len(dates)==1 and datetime.date.today()>=datetime.datetime.strptime(dates[0],'%m/%d/%Y').date(),'Source377 INF future/invalid date; no compile and no machine clock change')
 require(a.enable_candidate,'Explicit candidate opt-in');spec=read(HERE/'ge8300-gdi377-incremental-pins.json');commit=old.reuse.clean_source();rows=spec['rows']
 subprocess.run(['git','-C',ROOT,'merge-base','--is-ancestor',spec['runtime_base'],commit],check=True)
 expected={r['source'] for r in rows};allowed=expected|{'drivers/gpu/mt8167/wddm-render/ge8300-render.inf','drivers/gpu/mt8167/wddm-render/tests/gdi_present.py','drivers/gpu/mt8167/wddm-render/tests/standard_staging.py','drivers/gpu/mt8167/wddm-render/GDI-CONTRACT377.md'}
 changed=set(subprocess.check_output(['git','-C',ROOT,'diff','--name-only',spec['runtime_base'],commit,'--','drivers/gpu/mt8167'],text=True).splitlines());require(expected<=changed<=allowed,'Only two runtime TUs, INF and declared tests/docs may change')
 inputs,base,out=a.inputs.resolve(),a.baseline376.resolve(),a.output.resolve()
 for name,digest in spec['inputs'].items():pin(child(inputs,name),digest)
 pin(base/'incremental-build-manifest.json',spec['baseline376_manifest_sha256']);baseline=read(base/'incremental-build-manifest.json');require(baseline['success'] and baseline['source_commit']==spec['runtime_base'],'Actual376 source/build required')
 for name,digest in baseline['outputs'].items():pin(child(base,name),digest)
 for name,digest in spec['baseline376_archives'].items():pin(base/name,digest)
 archive=base/'ge8300-kmd-clear.lib';parsed=members(archive)
 for r in rows:require(sum(n==r['object'] for n,_ in parsed)==1,'Exact unique retained376 member required')
 plan=dict(schema='ge8300-gdi377-two-tu-build-v1',source_commit=commit,candidate_version=377,rows=rows,native_changed_TU=0,module_changed_TU=2,UMD_compiles=0,SDK_builds=0,transport_rebuilt=False,full_native_rebuilt=False,SYS_links=1,signed=False,hardware_verified=False,deployment_performed=False,baseline376_manifest_sha256=sha(base/'incremental-build-manifest.json'))
 if a.plan_only:print(json.dumps(dict(plan,executed=False),indent=2));return
 require(os.name=='nt','Windows build only');require(not out.exists() and out.is_relative_to(ROOT) and not out.is_relative_to(inputs) and not out.is_relative_to(base),'New source-local output required')
 link=a.vc_directory/'bin/Hostx64/arm/link.exe';pin(link,spec['LINK_sha256']);tools={n:a.llvm_directory/(n+'.exe') for n in ('clang','llvm-lib','llvm-readobj')}
 for name,tool in tools.items():pin(tool,spec['tool_pins'][name])
 km=inputs/'wdk81/Lib/winv6.3/km/arm'
 for name,digest in spec['link_inputs'].items():pin(km/name,digest)
 resource=Path(subprocess.check_output([tools['clang'],'-print-resource-dir'],text=True).strip())
 def relocated(path):
  s=re.sub('/+','/',str(path).replace('\\','/'));low=s.lower()
  if low.startswith('c:/gpu-gdi376/source/'):return ROOT/s[len('c:/gpu-gdi376/source/'):]
  if low.startswith('c:/gpu-gdi376/inputs/'):return inputs/s[len('c:/gpu-gdi376/inputs/'):]
  if '/llvm/lib/clang/' in low and '/include' in low:return resource/'include'/s.split('/include',1)[1].lstrip('/')
  # Retained system/compiler header paths must really exist and match376.
  p=Path(s);require(p.is_absolute(),'Unmapped relative376 path');return p
 oldrows={r['object']:r for r in baseline['new_compiles']};closure_pins={}
 for row in rows:
  prior=oldrows[row['object']];require(prior['arguments']==row['arguments'],'Actual376 arguments differ')
  for name,digest in prior['actual_include_closure'].items():
   p=relocated(name).resolve()
   if p==ROOT/row['source']:continue
   pin(p,digest);closure_pins[str(p)]=digest
 out.mkdir(parents=True);(out/'objects').mkdir();(out/'logs').mkdir();env=dict(os.environ,TMP=str(out/'logs'),TEMP=str(out/'logs'),VSLANG='1033')
 receipt=dict(plan,success=False,new_compiles=[],commands=[],reused_archives={},actual_retained376_header_pins=closure_pins,historical_include_closure_reconstructed=False)
 def run(args,name):
  q=subprocess.run(list(map(str,args)),cwd=ROOT,env=env,capture_output=True,text=True,errors='replace');p=out/'logs'/name;p.write_text(q.stdout+q.stderr);receipt['commands'].append(dict(arguments=list(map(str,args)),returncode=q.returncode,log=name,log_sha256=sha(p)));require(q.returncode==0,'Command failed: '+str(p));return q
 def compile_row(row):
  source=ROOT/row['source'];obj=out/'objects'/row['object'];dep=obj.with_suffix('.d');args=[x[:2]+str(relocated(x[2:])) if x.startswith(('-I','/I')) else x for x in row['arguments']]
  run([tools['clang'],*args,'-MD','-MF',dep,'-MT','ge8300-unit','-c',source,'-o',obj],obj.stem+'.log')
  headers=[Path(x).resolve() for x in old.reuse.dependencies(dep)];require(headers,'Actual include closure required')
  closure={str(h):sha(h) for h in [source,*headers]}
  require({str(p):h for p,h in closure.items() if Path(p)!=source.resolve()}=={str(relocated(p).resolve()):h for p,h in oldrows[row['object']]['actual_include_closure'].items() if relocated(p).resolve()!=source.resolve()},'Non-TU include closure must equal actual376 after relocation')
  receipt['new_compiles'].append(dict(source=row['source'],source_sha256=sha(source),object=obj.name,object_sha256=sha(obj),arguments=args,tool_sha256=sha(tools['clang']),actual_include_closure=closure));return obj
 try:
  with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:replacement={p.name:p for p in pool.map(compile_row,rows)}
  folder=out/'retained-kmd';folder.mkdir();objects=[];hashes={}
  for name,data in parsed:
   p=folder/name;p.write_bytes(data);p=replacement.get(name,p);objects.append(p);hashes[name]=sha(p)
  kmd=out/'ge8300-kmd-clear.lib';run([tools['llvm-lib'],'/machine:arm','/out:'+str(kmd),*reversed(objects)],'kmd-archive.log');actual=members(kmd)
  require([n for n,_ in actual]==[n for n,_ in parsed] and {n:hashlib.sha256(b).hexdigest() for n,b in actual}==hashes,'Archive order/member hash drift')
  native=out/'ge8300-native-owner-core.lib';shutil.copyfile(base/native.name,native);pin(native,spec['baseline376_archives'][native.name])
  receipt['reused_archives']={'kmd':dict(baseline_sha256=sha(archive),output_sha256=sha(kmd),replacement_members=sorted(replacement),members=hashes),'native':dict(baseline_sha256=sha(base/native.name),output_sha256=sha(native),replacement_members=[])}
  libs=[km/n for n in ('displib.lib','ntoskrnl.lib','hal.lib','libcntpr.lib','bufferoverflowfastfailk.lib')]
  run([link,'/largeaddressaware','/tsaware:no','/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/base:0x400000',kmd,native,*libs,'/out:'+str(out/'ge8300-render.sys'),'/map:'+str(out/'ge8300-render.map')],'render-link.log')
  run([tools['llvm-readobj'],'--file-headers','--coff-imports',out/'ge8300-render.sys'],'render-pe.log');receipt['actual_link_inputs']={str(p):sha(p) for p in [link,kmd,native,*libs]};receipt['retained_transport_sha256']=sha(inputs/'package372/display-transport/mt8167-display-transport.sys');receipt['retained_package_manifest_sha256']=sha(inputs/'package372/signed-manifest.json')
  consumed=dict(old.reuse.HASH_CACHE);old.reuse.HASH_CACHE.clear()
  for name,digest in consumed.items():pin(Path(name),digest)
  require(old.reuse.clean_source()==commit,'Source changed during build');receipt['success']=True
 finally:
  receipt['outputs']={p.relative_to(out).as_posix():sha(p) for p in out.rglob('*') if p.is_file()};(out/'incremental-build-manifest.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print('GDI377_TWO_TU_ONE_RENDER_LINK_NATIVE376_TRANSPORT372_UMD_FW_RETAINED')
if __name__=='__main__':main()
