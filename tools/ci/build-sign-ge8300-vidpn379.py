#!/usr/bin/env python3
"""保持378から表示bridge一単位だけを再コンパイルし、379を署名する。"""
import argparse,hashlib,importlib.util,json,os,re,shutil,subprocess
from pathlib import Path
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sp=importlib.util.spec_from_file_location('retained',HERE/'build-ge8300-gdi377-incremental.py')
old=importlib.util.module_from_spec(sp);sp.loader.exec_module(old)
from ge8300_panel_signing_common import unchanged_signed_payload
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
def check(ok,message):
 if not ok:raise ValueError(message)
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for n in ('inputs','deps','output'):ap.add_argument('--'+n,type=Path,required=True)
 ap.add_argument('--execute',action='store_true');a=ap.parse_args()
 spec=read(HERE/'ge8300-vidpn379-inputs.json');flagspec=read(HERE/'ge8300-gdi376-incremental-pins.json')
 rev=subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD'],text=True).strip()
 check(not subprocess.check_output(['git','-C',ROOT,'status','--porcelain'],text=True).strip(),'Clean committed source required')
 changed=set(subprocess.check_output(['git','-C',ROOT,'diff','--name-only',spec['baseline_source'],'--','drivers/gpu/mt8167'],text=True).splitlines())
 check(changed=={'drivers/gpu/mt8167/wddm-display/unified_display.cxx','drivers/gpu/mt8167/wddm-display/vidpn_callback_trace.h','drivers/gpu/mt8167/wddm-render/ge8300-render.inf'},'Exact bridge/header/INF changes required')
 inp=a.inputs.resolve();deps=a.deps.resolve();out=a.output.resolve()
 for n,v in spec['files'].items():check((inp/n).stat().st_size==v['bytes'] and sha(inp/n)==v['sha256'],'Pinned input mismatch: '+n)
 check(not out.exists() and out.is_relative_to(ROOT),'Fresh checkout-local output required')
 vc=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133');llvm=Path('C:/Program Files/LLVM/bin')
 tools={'cl':vc/'bin/Hostx64/arm/cl.exe','link':vc/'bin/Hostx64/arm/link.exe','lib':llvm/'llvm-lib.exe','readobj':llvm/'llvm-readobj.exe','sign':deps/'signing-tools/bin/signtool.exe','inf2cat':deps/'signing-tools/bin/Inf2Cat.exe'}
 check(sha(tools['cl'])==flagspec['CL_sha256'] and sha(tools['link'])==flagspec['LINK_sha256'],'Retained MSVC14.29 tools required')
 check(sha(tools['lib'])==flagspec['tool_pins']['llvm-lib'],'Retained archive tool required')
 wdk=deps/'gdi376-inputs/wdk81';km=wdk/'Lib/winv6.3/km/arm';libs=[]
 for n,h in flagspec['link_inputs'].items():check(sha(km/n)==h.lower(),'Kernel library drift: '+n);libs.append(km/n)
 receipt=dict(schema='GE8300_VIDPN379_SINGLE_TU_V1',source_commit=rev,trace_source_commit=spec['trace_source'],baseline_source_commit=spec['baseline_source'],client_source_commit=spec['client_source'],success=False,SDK_rebuilt=False,PCO_rebuilt=False,UMD_rebuilt=False,native_owner_rebuilt=False,compiled_TUs=[],steps={},tools={n:dict(path=str(p),sha256=sha(p)) for n,p in tools.items()},hardware_verified=False)
 if not a.execute:print(json.dumps(receipt,indent=2));return
 check(os.name=='nt','Windows build required');out.mkdir(parents=True);(out/'logs').mkdir();(out/'members').mkdir()
 save=lambda:(out/'build-sign-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 env=dict(os.environ,TMP=str(out/'logs'),TEMP=str(out/'logs'),VSLANG='1033')
 def run(name,args):
  receipt['steps'][name]={'arguments':list(map(str,args)),'exit_code':None};save()
  q=subprocess.run(list(map(str,args)),cwd=ROOT,env=env,capture_output=True,text=True,errors='replace');log=out/'logs'/(name+'.log');log.write_text(q.stdout+q.stderr)
  receipt['steps'][name].update(exit_code=q.returncode,log_sha256=sha(log));save();check(q.returncode==0,'Failed stage retained; no replay: '+str(log));return q.stdout+q.stderr
 try:
  row=next(r for r in flagspec['rows'] if r['object']=='display_unified_display.obj');args=[]
  for flag in row['arguments']:
   if not flag.startswith('/I'):args.append(flag);continue
   path=flag[2:].replace('\\','/');low=path.lower()
   if '/windows kits/8.1/include/' in low:path=wdk/'Include'/path[low.index('/include/')+9:]
   elif '/windows kits/10/include/' in low:path=deps/'crt-headers/ucrt'
   elif '/vc/tools/msvc/' in low:path=vc/'include'
   elif '/drivers/gpu/mt8167/' in low:path=ROOT/'drivers/gpu/mt8167'/path[low.index('/drivers/gpu/mt8167/')+len('/drivers/gpu/mt8167/'):]
   else:raise ValueError('Unmapped original include: '+path)
   args.append('/I'+str(path))
  source=ROOT/'drivers/gpu/mt8167/wddm-display/unified_display.cxx';obj=out/row['object']
  text=run('compile-bridge',[tools['cl'],*args,'/showIncludes','/Fo'+str(obj),source]);closure={str(source):sha(source)}
  for line in text.splitlines():
   hit=re.match(r'^Note: including file:\s*(.+)$',line)
   if hit:p=Path(hit[1].strip()).resolve();closure[str(p)]=sha(p)
  check(len(closure)>10 and any(n.endswith('vidpn_callback_trace.h') for n in closure),'Real include closure missing')
  receipt['compiled_TUs']=[dict(source=str(source.relative_to(ROOT)),object=obj.name,sha256=sha(obj),include_closure=closure)];save()
  before=old.members(inp/'baseline378/ge8300-kmd-clear.lib');check(sum(n==obj.name for n,b in before)==1,'Single retained bridge member required');objects=[];expected={}
  for n,b in before:
   check(Path(n).name==n,'Unsafe archive member');p=out/'members'/n;p.write_bytes(b);p=obj if n==obj.name else p;objects.append(p);expected[n]=sha(p)
  lib=out/'ge8300-kmd-clear.lib';run('archive',[tools['lib'],'/machine:arm','/out:'+str(lib),*reversed(objects)])
  after=old.members(lib);check([n for n,b in before]==[n for n,b in after] and {n:hashlib.sha256(b).hexdigest() for n,b in after}==expected,'Archive member/order drift')
  receipt['archive']={'baseline_sha256':sha(inp/'baseline378/ge8300-kmd-clear.lib'),'sha256':sha(lib),'replacement':'display_unified_display.obj','members':expected}
  sys=out/'ge8300-render.sys';run('kernel-link',[tools['link'],'/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000',lib,inp/'baseline378/ge8300-native-owner-core.lib',*libs,'/out:'+str(sys),'/map:'+str(out/'ge8300-render.map')])
  run('pe',[tools['readobj'],'--file-headers','--coff-imports',sys]);pkg=out/'package379';shutil.copytree(inp/'package378',pkg)
  (pkg/'signed-manifest.json').unlink();(pkg/'ge8300-render.cat').unlink();shutil.copyfile(sys,pkg/sys.name);shutil.copyfile(ROOT/'drivers/gpu/mt8167/wddm-render/ge8300-render.inf',pkg/'ge8300-render.inf');shutil.copyfile(inp/'client/mt8167-dxgi-present356.exe',pkg/'mt8167-dxgi-present356.exe')
  thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';run('sign-sys',[tools['sign'],'sign','/fd','SHA256','/s','My','/sha1',thumb,pkg/sys.name]);unchanged_signed_payload(sys.read_bytes(),(pkg/sys.name).read_bytes())
  transport=out/'retained-transport';(pkg/'display-transport').rename(transport)
  try:run('catalog',[tools['inf2cat'],'/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime'])
  finally:transport.rename(pkg/'display-transport')
  run('sign-cat',[tools['sign'],'sign','/fd','SHA256','/s','My','/sha1',thumb,pkg/'ge8300-render.cat'])
  signed=[pkg/n for n in ('ge8300-render.sys','ge8300-render.cat','ge8300-umd.dll','mt8167-dxgi-present356.exe','display-transport/mt8167-display-transport.sys','display-transport/mt8167-display-transport.cat')]
  for i,p in enumerate(signed):run('trust-'+str(i),[tools['sign'],'verify','/pa','/v',p])
  for n in ('ge8300-render.sys','ge8300-render.inf','ge8300-umd.dll','ge8300-mips.fw'):run('catalog-'+n,[tools['sign'],'verify','/pa','/c',pkg/'ge8300-render.cat',pkg/n])
  paths=out/'signature-paths.json';paths.write_text(json.dumps(list(map(str,signed))))
  run('certificates',['powershell.exe','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',HERE/'verify-dwm-certificates.ps1','-PathsJson',paths,'-ExpectedThumbprint',thumb,'-ReportJson',out/'certificate-report.json'])
  baseline=read(inp/'package378/signed-manifest.json');actual={p.relative_to(pkg).as_posix():sha(p) for p in pkg.rglob('*') if p.is_file()};changes={'ge8300-render.sys','ge8300-render.inf','ge8300-render.cat','mt8167-dxgi-present356.exe'}
  check(set(actual)=={r['name'] for r in baseline['outputs']},'Package member drift')
  for r in baseline['outputs']:check((actual[r['name']]!=r['sha256'])==(r['name'] in changes),'Unexpected package change: '+r['name'])
  for n,h in closure.items():check(sha(Path(n))==h,'Compile include drift')
  for n,v in spec['files'].items():check(sha(inp/n)==v['sha256'],'Input drift')
  check(subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD'],text=True).strip()==rev and not subprocess.check_output(['git','-C',ROOT,'status','--porcelain'],text=True).strip(),'Source drift')
  receipt.update(success=True,signed=True,signer_thumbprint=thumb,candidate_version=379,runtime_build_id_retained_from378=True,native_owner_sha256=sha(inp/'baseline378/ge8300-native-owner-core.lib'),outputs=[dict(name=n,sha256=h,bytes=(pkg/n).stat().st_size) for n,h in sorted(actual.items())]);save();(pkg/'signed-manifest.json').write_text(json.dumps(receipt,indent=2)+'\n')
 finally:save()
if __name__=='__main__':main()
