#!/usr/bin/env python3
"""実1032 objects41保持＋privacy EtherType境界2TUだけをcompile/link/sign。"""
import argparse,datetime,hashlib,json,os,re,shutil,subprocess
from pathlib import Path,PureWindowsPath
from ge8300_panel_signing_common import unchanged_signed_payload
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
def require(v,m):
 if not v:raise ValueError(m)
def main():
 ap=argparse.ArgumentParser(description=__doc__)
 for n in ('inputs','deps','output','baseline1032'):ap.add_argument('--'+n,type=Path,required=True)
 ap.add_argument('--execute',action='store_true');a=ap.parse_args();inp=a.inputs.resolve();deps=a.deps.resolve();out=a.output.resolve()
 spec=read(HERE/'mt6630-provider1032-inputs.json');pin=read(HERE/'mt6630-provider1033-inputs.json');baseline=read(inp/'baseline1031/signed-manifest.json')
 base32=a.baseline1032.resolve();manifest32=base32/'package/provider/signed-manifest.json';b32=read(manifest32)
 rev=subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD'],text=True).strip()
 require(not subprocess.check_output(['git','-C',ROOT,'status','--porcelain'],text=True).strip(),'Clean committed source required')
 require(sha(inp/'baseline1031/signed-manifest.json')==spec['baseline_manifest_sha256'] and baseline['schema']=='mt6630-provider1031-incremental-v1' and baseline['source_commit']==spec['baseline_source'],'Actual1031 baseline')
 require(datetime.date.today()>=datetime.date(2026,11,11),'No future INF / no clock change')
 require(sha(HERE/'mt6630-provider1032-inputs.json')==pin['inherited_input_spec_sha256'],'Exact accepted1032 input spec')
 require(sha(manifest32)==pin['baseline1032_manifest_sha256'] and b32['schema']=='mt6630-provider1032-rx-frequency-single-tu-v1' and b32['source_commit']==pin['baseline_source'] and b32['success'] is True and b32['signed'] is True,'Actual successful signed1032 baseline')
 require(b32['baseline_manifest_sha256']==spec['baseline_manifest_sha256'] and b32['baseline_source_commit']==spec['baseline_source'],'1032 to1031 provenance chain')
 require(b32['legacy_checkpoint_build']==1031 and b32['checkpoint_literal_relabelled'] is False,'Keep real CP literal1031')
 require(all(x['exit_code']==0 for x in b32['steps'].values()) and len(b32['steps'])==12,'All actual1032 steps completed')
 targets=set(pin['changed_TUs']);receive='drivers/network/mt6630/native_receive.c'
 changed=set(subprocess.check_output(['git','-C',ROOT,'diff','--name-only',pin['baseline_source'],'--','drivers/network/mt6630'],text=True).splitlines())
 require(changed==targets|{'drivers/network/mt6630/native_privacy.h','drivers/network/mt6630/mt6630-combo-runtime.inf'},'Only two privacy TUs, inline header and INF may change')
 for rel,h in pin['runtime_files'].items():require(sha(ROOT/rel)==h,'Exact reviewed runtime source '+rel)
 for r in spec['files']:
  p=inp/r['path'];require(p.stat().st_size==r['bytes'] and sha(p)==r['sha256'],'Retained1031 input '+r['path'])
 for row in b32['files']:
  p=base32/'package/provider'/row['path'];require(p.stat().st_size==row['bytes'] and sha(p)==row['sha256'],'Actual1032 signed package leaf')
 oldinf=(base32/'package/provider/mt6630-combo-runtime.inf').read_bytes();inf=ROOT/'drivers/network/mt6630/mt6630-combo-runtime.inf'
 require(inf.read_bytes()==oldinf.replace(b'2026.11.11.1032',b'2026.11.11.1033'),'Only formal1033 INF version delta')
 oldrows={r['object']:r for r in baseline['objects']};rows={r['object']:r for r in b32['objects']}
 require(len(rows)==len(oldrows)==len(b32['objects'])==43 and list(rows)==list(oldrows),'Exactly43 objects in accepted order')
 require({r['source'] for r in rows.values()}=={r['source'] for r in oldrows.values()},'Same source/object roles')
 require(len(b32['compiled_TUs'])==1,'1032 compiled receive only')
 rx=b32['compiled_TUs'][0]
 require(rx['source']==receive and rx['object']=='provider/native_receive.obj' and rx['sha256']==rows[rx['object']]['sha256'] and rx['sha256']!=oldrows[rx['object']]['sha256'],'Keep new1032 receive, never old1031 receive object')
 args=b32['steps']['compile-native-receive']['arguments'];old_source=PureWindowsPath(args[args.index('-c')+1]);old_root=old_source
 for _ in PureWindowsPath(receive).parts:old_root=old_root.parent
 require(old_source.relative_to(old_root).as_posix()==receive,'Actual1032 source root binding')
 inherited={};checks={}
 def bind(path,h,size=None):
  p=path.resolve();require(sha(p)==h and (size is None or p.stat().st_size==size),'Inherited source/include '+str(p));checks[str(p)]=dict(sha256=h,bytes=p.stat().st_size)
 for obj,row in rows.items():
  original=oldrows[obj];require(row['source']==original['source'],'Exact object/source role')
  p=base32/'objects'/obj;require(sha(p)==row['sha256'],'Actual1032 object '+obj)
  checks[str(p)]=dict(sha256=row['sha256'],bytes=p.stat().st_size)
  if row['source'] in targets:continue
  if row['source']==receive:
   require(row['compiled'] is True and row['compiled_source_commit']==pin['baseline_source'],'Actual compiled1032 receive provenance')
   repo={};external={}
   for name,v in rx['actual_include_closure'].items():
    w=PureWindowsPath(name)
    if w.is_relative_to(old_root):
     rel=w.relative_to(old_root).as_posix();bind(ROOT/rel,v['sha256'],v['bytes']);repo[rel]=v['sha256']
    else:bind(Path(name),v['sha256'],v['bytes']);external[name]=v
   require(receive in repo and len(repo)>1 and len(external)>5,'Full actual1032 receive closure')
   inherited[obj]=dict(provenance='1032 compiled_TU.actual_include_closure',repo_source_closure=repo,external_include_closure=external)
  else:
   require(row['compiled'] is False and row['source_closure_retained'] is True and row['sha256']==original['sha256'] and row['baseline_object_sha256']==original['sha256'] and row['compiled_source_commit']==original['compiled_source_commit'],'Exact1031 to1032 retained object chain '+obj)
   require(row['source'] in original['source_closure'],'Original TU present in closure')
   for rel,h in original['source_closure'].items():bind(ROOT/rel,h)
   inherited[obj]=dict(provenance='accepted1031 source_closure + actual1032 object hash',repo_source_closure=original['source_closure'])
 require(len(inherited)==41 and len([r for r in rows.values() if r['source'] in targets])==2,'Exactly41 retained and2 changed TUs')
 vc=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133')
 tools=dict(clang=Path('C:/Program Files/LLVM/bin/clang.exe'),link=vc/'bin/Hostx64/arm/link.exe',readobj=Path('C:/Program Files/LLVM/bin/llvm-readobj.exe'),signtool=deps/'signing-tools/bin/signtool.exe',inf2cat=deps/'signing-tools/bin/Inf2Cat.exe')
 for n,p in tools.items():require(sha(p)==spec['tools'][n] and b32['tools'][n]['sha256']==spec['tools'][n],'Existing exact1032 tool '+n)
 require(not out.exists() and out.is_relative_to(ROOT),'Fresh checkout-local output')
 flags=[];oldflags=iter(baseline['flags']);wdk=deps/'gdi376-inputs/wdk81/Include'
 for f in oldflags:
  if f.startswith('-IC:') and 'overlay' in f:flags.append('-I'+str(inp/'cache/small/overlay'))
  elif f=='-isystem':
   p=next(oldflags).replace('\\','/');suffix=p.lower().split('/include/',1)[1]
   flags.extend(['-isystem',str((inp/'cache/small/wdk/Include'/suffix) if suffix.lower().startswith('wdf/') else (wdk/suffix))])
  else:flags.append(f)
 # Same flags/path mapping as accepted1032; no compiler or SDK rebuild.
 def normalized_flag(f):
  if f.startswith('-I') and PureWindowsPath(f[2:]).is_absolute():return '-I'+PureWindowsPath(f[2:]).as_posix()
  if PureWindowsPath(f).is_absolute():return PureWindowsPath(f).as_posix()
  return f
 require(list(map(normalized_flag,flags))==list(map(normalized_flag,args[1:args.index('-MD')])),'Exact actual1032 compile flags and SDK view')
 receipt=dict(schema='mt6630-provider1033-privacy-network-order-two-tu-v1',source_commit=rev,runtime_fix_source=pin['runtime_fix_source'],retained_receive_fix_source='b685c4f596d95490436008c69dc8e43f743d80c3',baseline_source_commit=b32['source_commit'],baseline_manifest_sha256=pin['baseline1032_manifest_sha256'],original1031_manifest_sha256=spec['baseline_manifest_sha256'],inherited_source_closures=inherited,version='2026.11.11.1033',driver_date='11/11/2026',legacy_checkpoint_build=1031,checkpoint_literal_relabelled=False,success=False,signed=False,compiled_TUs=[],reused_TUs=41,steps={},tools={n:dict(path=str(p),sha256=sha(p)) for n,p in tools.items()},SDK_rebuilt=False,PCO_rebuilt=False,GPU_rebuilt=False,parent_rebuilt=False,UMD_rebuilt=False,historical_system_include_equality_verified=False,hardware_verified=False,association_verified=False)
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
  closure={}
  receipt['flags']=flags;save()
  for target in sorted(targets):
   row=next(r for r in rows.values() if r['source']==target);source=ROOT/target;obj=out/'objects'/row['object'];dep=out/(source.stem+'.d')
   run('compile-'+source.stem,[tools['clang'],*flags,'-MD','-MF',dep,'-c',source,'-o',obj])
   flat=re.sub(r'\\\r?\n',' ',dep.read_text());parts=re.split(r':\s+',flat,maxsplit=1);require(len(parts)==2,'Actual clang dependency file')
   unit={}
   for word in re.split(r'(?<!\\)\s+',parts[1].strip()):
    if word:
     p=Path(word.replace('\\ ',' ').replace('\\#','#')).resolve();unit[str(p)]=dict(bytes=p.stat().st_size,sha256=sha(p))
   require(len(unit)>5 and str(source.resolve()) in unit and str((ROOT/'drivers/network/mt6630/native_privacy.h').resolve()) in unit,'Actual two-TU full include closure and shared helper')
   closure.update(unit);receipt['compiled_TUs'].append(dict(source=target,object=row['object'],sha256=sha(obj),actual_include_closure=unit));save()
  objects=[];records=[]
  for row in b32['objects']:
   p=out/'objects'/row['object']
   if row['source'] not in targets:shutil.copyfile(base32/'objects'/row['object'],p);require(sha(p)==row['sha256'],'Retained1032 object changed')
   records.append(dict(source=row['source'],object=row['object'],sha256=sha(p),compiled=row['source'] in targets,compiled_source_commit=rev if row['source'] in targets else row['compiled_source_commit'],baseline_object_sha256=row['sha256'],source_closure_retained=row['source'] not in targets));objects.append(p)
  libs=[]
  for row in spec['kernel_libraries']:
   p=inp/row['path'];require(sha(p)==row['sha256'],'Exact1031 KM library');libs.append(p)
  def recheck():
   for n,r in {**checks,**closure}.items():require(Path(n).stat().st_size==r['bytes'] and sha(Path(n))==r['sha256'],'Actual inherited/new input changed '+n)
   require(sha(manifest32)==pin['baseline1032_manifest_sha256'],'Baseline1032 manifest changed')
   for n,p in tools.items():require(sha(p)==spec['tools'][n],'Exact tool changed '+n)
   for row in records:require(sha(out/'objects'/row['object'])==row['sha256'],'Link input object changed')
   for r in spec['files']:require(sha(inp/r['path'])==r['sha256'],'Retained input changed')
  recheck();pkg=out/'package/provider';pkg.mkdir(parents=True);sys=pkg/'mt6630-combo-runtime.sys'
  run('kernel-link',[tools['link'],'/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/opt:ref','/incremental:no',*objects,*libs,'/out:'+str(sys),'/map:'+str(out/'provider.map')]);recheck()
  oldpe=run('baseline-pe',[tools['readobj'],'--file-headers','--coff-imports',base32/'package/provider/mt6630-combo-runtime.sys']);newpe=run('candidate-pe',[tools['readobj'],'--file-headers','--coff-imports',sys])
  symbols=lambda s:sorted(l.strip() for l in s.splitlines() if re.match(r'^\s*(Name|Symbol):',l))
  require('IMAGE_FILE_MACHINE_ARMNT' in newpe and symbols(oldpe)==symbols(newpe) and symbols(newpe),'ARMNT and identical1032 import symbol set')
  unsigned=out/'unsigned-mt6630-combo-runtime.sys';shutil.copyfile(sys,unsigned);shutil.copyfile(inf,pkg/inf.name)
  thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
  run('sign-sys',[tools['signtool'],'sign','/fd','SHA256','/s','My','/sha1',thumb,sys]);unchanged_signed_payload(unsigned.read_bytes(),sys.read_bytes())
  run('catalog',[tools['inf2cat'],'/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime']);cat=pkg/'mt6630-combo-runtime.cat';run('sign-cat',[tools['signtool'],'sign','/fd','SHA256','/s','My','/sha1',thumb,cat])
  for n,p in (('sys',sys),('cat',cat)):run('verify-'+n,[tools['signtool'],'verify','/pa','/v',p])
  for n,p in (('sys',sys),('inf',pkg/inf.name)):run('membership-'+n,[tools['signtool'],'verify','/pa','/c',cat,p])
  paths=out/'signature-paths.json';paths.write_text(json.dumps([str(sys),str(cat)]))
  run('certificates',['powershell.exe','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',HERE/'verify-dwm-certificates.ps1','-PathsJson',paths,'-ExpectedThumbprint',thumb,'-ReportJson',out/'certificate-report.json'])
  recheck();require(subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD'],text=True).strip()==rev and not subprocess.check_output(['git','-C',ROOT,'status','--porcelain'],text=True).strip(),'Source changed')
  receipt.update(success=True,signed=True,signature_status='Valid',signer_thumbprint=thumb,objects=records,object_order_retained=True,imports_equal_actual1032=True,unsigned_SYS_sha256=sha(unsigned),non_signature_payload_unchanged=True,files=[dict(path=p.name,bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(pkg.iterdir())]);save();(pkg/'signed-manifest.json').write_text(json.dumps(receipt,indent=2)+'\n')
 finally:save()
if __name__=='__main__':main()
