#!/usr/bin/env python3
"""実1031 objects42保持＋Native RX OOB境界1TUだけをcompile/link/sign。"""
import argparse,datetime,hashlib,json,os,re,shutil,subprocess
from pathlib import Path
from ge8300_panel_signing_common import unchanged_signed_payload
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
def require(v,m):
 if not v:raise ValueError(m)
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for n in ('inputs','deps','output'):ap.add_argument('--'+n,type=Path,required=True)
 ap.add_argument('--execute',action='store_true');a=ap.parse_args();inp=a.inputs.resolve();deps=a.deps.resolve();out=a.output.resolve()
 spec=read(HERE/'mt6630-provider1032-inputs.json');baseline=read(inp/'baseline1031/signed-manifest.json')
 rev=subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD'],text=True).strip()
 require(not subprocess.check_output(['git','-C',ROOT,'status','--porcelain'],text=True).strip(),'Clean committed source required')
 require(sha(inp/'baseline1031/signed-manifest.json')==spec['baseline_manifest_sha256'] and baseline['schema']=='mt6630-provider1031-incremental-v1' and baseline['source_commit']==spec['baseline_source'],'Actual1031 baseline')
 require(datetime.date.today()>=datetime.date(2026,11,11),'No future INF / no clock change')
 changed=set(subprocess.check_output(['git','-C',ROOT,'diff','--name-only',spec['baseline_source'],'--','drivers/network/mt6630'],text=True).splitlines())
 require(changed=={'drivers/network/mt6630/native_receive.c','drivers/network/mt6630/mt6630-combo-runtime.inf'},'Only RX indication and INF may change')
 for r in spec['files']:
  p=inp/r['path'];require(p.stat().st_size==r['bytes'] and sha(p)==r['sha256'],'Retained input '+r['path'])
 require(len(baseline['objects'])==43 and len({x['object'] for x in baseline['objects']})==43,'Exactly43 original objects')
 target='drivers/network/mt6630/native_receive.c';source=ROOT/target
 require(sha(source)==spec['native_receive_sha256'],'Reviewed b685 native receive source')
 oldinf=(inp/'baseline1031/mt6630-combo-runtime.inf').read_bytes();inf=ROOT/'drivers/network/mt6630/mt6630-combo-runtime.inf'
 require(inf.read_bytes()==oldinf.replace(b'DriverVer=11/10/2026,2026.11.11.1031',b'DriverVer=11/11/2026,2026.11.11.1032'),'Only formal INF version/date delta')
 for row in baseline['objects']:
  require(sha(inp/'objects'/row['object'])==row['sha256'],'Actual retained object '+row['object'])
  if row['source']!=target:
   for rel,h in row['source_closure'].items():require(sha(ROOT/rel)==h,'All42 reused TU source closures strict: '+rel)
 vc=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133')
 tools=dict(clang=Path('C:/Program Files/LLVM/bin/clang.exe'),link=vc/'bin/Hostx64/arm/link.exe',readobj=Path('C:/Program Files/LLVM/bin/llvm-readobj.exe'),signtool=deps/'signing-tools/bin/signtool.exe',inf2cat=deps/'signing-tools/bin/Inf2Cat.exe')
 for n,p in tools.items():require(sha(p)==spec['tools'][n],'Existing exact tool '+n)
 require(not out.exists() and out.is_relative_to(ROOT),'Fresh checkout-local output')
 receipt=dict(schema='mt6630-provider1032-rx-frequency-single-tu-v1',source_commit=rev,runtime_fix_source='b685c4f596d95490436008c69dc8e43f743d80c3',baseline_source_commit=baseline['source_commit'],baseline_manifest_sha256=spec['baseline_manifest_sha256'],version='2026.11.11.1032',driver_date='11/11/2026',legacy_checkpoint_build=1031,checkpoint_literal_relabelled=False,success=False,signed=False,compiled_TUs=[],reused_TUs=42,steps={},tools={n:dict(path=str(p),sha256=sha(p)) for n,p in tools.items()},SDK_rebuilt=False,PCO_rebuilt=False,GPU_rebuilt=False,parent_rebuilt=False,UMD_rebuilt=False,historical_system_include_equality_verified=False,hardware_verified=False,association_verified=False)
 if not a.execute:print(json.dumps(receipt,indent=2));return
 require(os.name=='nt','Windows execution only');out.mkdir(parents=True);(out/'logs').mkdir();(out/'objects/provider').mkdir(parents=True)
 save=lambda:(out/'build-sign-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 env=dict(os.environ,TMP=str(out/'logs'),TEMP=str(out/'logs'),VSLANG='1033')
 def run(label,args):
  require(label not in receipt['steps'],'No automatic replay of executed stage')
  receipt['steps'][label]=dict(arguments=list(map(str,args)),exit_code=None);save()
  r=subprocess.run(list(map(str,args)),cwd=ROOT,env=env,capture_output=True,text=True,errors='replace');log=out/'logs'/(label+'.log');log.write_text(r.stdout+r.stderr)
  receipt['steps'][label].update(exit_code=r.returncode,log_sha256=sha(log));save();require(r.returncode==0,'Stage failed; retain outputs: '+label);return r.stdout+r.stderr
 try:
  flags=[];oldflags=iter(baseline['flags']);wdk=deps/'gdi376-inputs/wdk81/Include'
  for f in oldflags:
   if f.startswith('-IC:') and 'overlay' in f:flags.append('-I'+str(inp/'cache/small/overlay'))
   elif f=='-isystem':
    p=next(oldflags).replace('\\','/');suffix=p.lower().split('/include/',1)[1]
    flags.extend(['-isystem',str((inp/'cache/small/wdk/Include'/suffix) if suffix.lower().startswith('wdf/') else (wdk/suffix))])
   else:flags.append(f)
  obj=out/'objects/provider/native_receive.obj';dep=out/'native_receive.d'
  run('compile-native-receive',[tools['clang'],*flags,'-MD','-MF',dep,'-c',source,'-o',obj])
  flat=re.sub(r'\\\r?\n',' ',dep.read_text());parts=re.split(r':\s+',flat,maxsplit=1);require(len(parts)==2,'Actual clang dependency file')
  closure={}
  for word in re.split(r'(?<!\\)\s+',parts[1].strip()):
   if word:
    p=Path(word.replace('\\ ',' ').replace('\\#','#')).resolve();closure[str(p)]=dict(bytes=p.stat().st_size,sha256=sha(p))
  require(len(closure)>5 and str(source.resolve()) in closure,'Actual new TU full include closure')
  receipt['compiled_TUs']=[dict(source=target,object='provider/native_receive.obj',sha256=sha(obj),actual_include_closure=closure)];save()
  objects=[];records=[]
  for row in baseline['objects']:
   p=out/'objects'/row['object']
   if row['source']!=target:shutil.copyfile(inp/'objects'/row['object'],p);require(sha(p)==row['sha256'],'Retained object changed')
   records.append(dict(source=row['source'],object=row['object'],sha256=sha(p),compiled=row['source']==target,compiled_source_commit=rev if row['source']==target else row['compiled_source_commit'],baseline_object_sha256=row['sha256'],source_closure_retained=row['source']!=target));objects.append(p)
  libs=[]
  for row in spec['kernel_libraries']:
   p=inp/row['path'];require(sha(p)==row['sha256'],'Exact1031 KM library');libs.append(p)
  def recheck():
   for n,r in closure.items():require(sha(Path(n))==r['sha256'],'Actual include changed '+n)
   for r in spec['files']:require(sha(inp/r['path'])==r['sha256'],'Retained input changed')
  recheck();pkg=out/'package/provider';pkg.mkdir(parents=True);sys=pkg/'mt6630-combo-runtime.sys'
  run('kernel-link',[tools['link'],'/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/opt:ref','/incremental:no',*objects,*libs,'/out:'+str(sys),'/map:'+str(out/'provider.map')]);recheck()
  oldpe=run('baseline-pe',[tools['readobj'],'--file-headers','--coff-imports',inp/'baseline1031/mt6630-combo-runtime.sys']);newpe=run('candidate-pe',[tools['readobj'],'--file-headers','--coff-imports',sys])
  symbols=lambda s:sorted(l.strip() for l in s.splitlines() if re.match(r'^\s*(Name|Symbol):',l))
  require('IMAGE_FILE_MACHINE_ARMNT' in newpe and symbols(oldpe)==symbols(newpe) and symbols(newpe),'ARMNT and identical1031 import symbol set')
  unsigned=out/'unsigned-mt6630-combo-runtime.sys';shutil.copyfile(sys,unsigned);shutil.copyfile(inf,pkg/inf.name)
  thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
  run('sign-sys',[tools['signtool'],'sign','/fd','SHA256','/s','My','/sha1',thumb,sys]);unchanged_signed_payload(unsigned.read_bytes(),sys.read_bytes())
  run('catalog',[tools['inf2cat'],'/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime']);cat=pkg/'mt6630-combo-runtime.cat';run('sign-cat',[tools['signtool'],'sign','/fd','SHA256','/s','My','/sha1',thumb,cat])
  for n,p in (('sys',sys),('cat',cat)):run('verify-'+n,[tools['signtool'],'verify','/pa','/v',p])
  for n,p in (('sys',sys),('inf',pkg/inf.name)):run('membership-'+n,[tools['signtool'],'verify','/pa','/c',cat,p])
  paths=out/'signature-paths.json';paths.write_text(json.dumps([str(sys),str(cat)]))
  run('certificates',['powershell.exe','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',HERE/'verify-dwm-certificates.ps1','-PathsJson',paths,'-ExpectedThumbprint',thumb,'-ReportJson',out/'certificate-report.json'])
  recheck();require(subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD'],text=True).strip()==rev and not subprocess.check_output(['git','-C',ROOT,'status','--porcelain'],text=True).strip(),'Source changed')
  receipt.update(success=True,signed=True,signature_status='Valid',signer_thumbprint=thumb,objects=records,object_order_retained=True,imports_equal_actual1031=True,unsigned_SYS_sha256=sha(unsigned),non_signature_payload_unchanged=True,files=[dict(path=p.name,bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(pkg.iterdir())]);save();(pkg/'signed-manifest.json').write_text(json.dumps(receipt,indent=2)+'\n')
 finally:save()
if __name__=='__main__':main()
