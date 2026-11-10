#!/usr/bin/env python3
"""Three-TU diagnostic-only359 from immutable358 cache; no device operations."""
from pathlib import Path
import argparse,json,hashlib,subprocess,shutil,copy,re,zipfile
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True);p.add_argument('--source-commit',required=True);a=p.parse_args();t=a.task_root;r=t/'source';b=Path('C:/mt8167-segmented358/build');o=t/'build';pkg=o/'package359';llvm=Path('C:/Program Files/LLVM/bin');msvc=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm');sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();load=lambda p:json.loads(Path(p).read_text(encoding='utf-8-sig'))
def run(cmd,name):
 q=subprocess.run([str(x)for x in cmd],capture_output=True,text=True);(o/'logs'/name).write_text(q.stdout+q.stderr)
 if q.returncode:raise RuntimeError(name+' '+q.stdout[-1400:]+q.stderr[-1400:])
 return q.stdout+q.stderr
assert subprocess.check_output(['git','-C',str(r),'rev-parse','HEAD'],text=True).strip()==a.source_commit
assert not subprocess.check_output(['git','-C',str(r),'status','--porcelain'],text=True).strip()
subprocess.run(['python',str(r/'tools/ci/verify-ge8300-fullsource.py')],check=True)
assert sha(b/'package358/signed-manifest.json')=='901e32a09443b9910b3c4a0c6ed320a70ad1ae5f3fe319086fdcd61efc1a81e8';old=load(b/'package358/signed-manifest.json')
for x in old['outputs']:assert sha(b/'package358'/x['name'])==x['sha256']
for ar in old['archives']:
 assert sha(b/ar['file'])==ar['sha256']
 for x in ar['members']:
  if ar['role']in ('native','kmd'):assert sha(b/(ar['role']+'-objects')/x['name'])==x['sha256']
assert not o.exists();shutil.copytree(b,o);(o/'package358').rename(pkg);m=copy.deepcopy(old)
expected={'drivers/gpu/mt8167/wddm-render/miniport.c','drivers/gpu/mt8167/wddm-render/display_adapter356.c','drivers/gpu/mt8167/wddm-display/bdd.cxx'};header='drivers/gpu/mt8167/start_gap359.h'
changed=set(subprocess.check_output(['git','-C',str(r),'diff','--name-only','efee1f9b9e9226a14f06e900815f4edc9555d084','HEAD'],text=True).splitlines());assert {x for x in changed if x.startswith('drivers/')}==expected|{header}
closures={u['source']:copy.deepcopy(u)for u in old['actual_include_closure']};selected=[]
for u in m['compiled_units']:
 if u['source']not in expected:continue
 src=r/u['source'];flags=[]
 for arg in u['arguments']:
  arg=re.sub('/+','/',str(arg).replace('\\','/'))
  for oldroot in ['C:/mt8167-present356/source','C:/mt8167-start357/source','C:/mt8167-segmented358/source']:arg=arg.replace(oldroot,r.as_posix())
  flags.append(arg)
 obj=o/'kmd-objects'/u['object']if u['role']=='kmd'else o/u['object'];local={u['source']}
 if u['source'].endswith('.cxx'):
  log=run([msvc/'cl.exe',*flags,'/showIncludes',src,'/Fo'+str(obj)],u['object']+'.compile359.log')
  lines=[x.split('including file:',1)[1].strip()for x in log.splitlines()if 'including file:'in x]
 else:
  pp=subprocess.run([str(llvm/'clang.exe'),*flags,'-H','-E',str(src),'-o','NUL'],capture_output=True,text=True);(o/'logs'/(u['object']+'.include359.log')).write_text(pp.stderr);pp.check_returncode();lines=[x.group(1).strip()for line in pp.stderr.splitlines()if(x:=re.match(r'^\.+ (.+)$',line))]
  run([llvm/'clang.exe',*flags,'-c',src,'-o',obj],u['object']+'.compile359.log')
 for h in lines:
  try:local.add(Path(h).resolve().relative_to(r.resolve()).as_posix())
  except ValueError:pass
 assert header in local,'New header include not recorded '+u['source']
 entry=dict(source=u['source'],source_sha256=sha(src),object=u['object'],object_sha256=sha(obj),role=u['role'],arguments=flags,actual_compile_source_commit=a.source_commit);selected.append(entry);u.update(entry);closures[u['source']]=dict(source=u['source'],role=u['role'],local_closure={n:sha(r/n)for n in sorted(local)},compile_required=True)
assert len(selected)==3
for u in closures.values():
 if u['source']not in expected:assert not(set(u['local_closure'])&(expected|{header})),'Omitted affected TU'
archives=[]
for ar in old['archives']:
 new=copy.deepcopy(ar)
 if ar['role']=='kmd':
  d=o/'kmd-objects';run([llvm/'llvm-ar.exe','rcs',o/ar['file'],*[d/x['name']for x in ar['members']]],'kmd.archive359.log');new['sha256']=sha(o/ar['file'])
  for x in new['members']:x['sha256']=sha(d/x['name'])
 else:assert sha(o/ar['file'])==ar['sha256']
 archives.append(new)
rawold={x['name']:x['sha256']for ar in old['archives']if ar['role']in ('native','kmd')for x in ar['members']};rawnew={x['name']:x['sha256']for ar in archives if ar['role']in ('native','kmd')for x in ar['members']};delta=[n for n in rawold if rawold[n]!=rawnew[n]];assert delta==['miniport.obj']or set(delta)=={'miniport.obj'}
kit=Path('C:/mt8167-public-fullsource323/wdk81/Lib/winv6.3/km/arm');link=msvc/'link.exe';assert sha(link)=='f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938'
flags=['/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000'];libs=[kit/(n+'.lib')for n in ['displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk']];bdd=[o/('display_'+n+'.obj')for n in ['bdd','bdd_dmm','bdd_util','bltfuncs','blthw','mt8167_brightness','unified_display','memory','ge8300_trace','ge8300_present']]
run([link,*flags,o/'ge8300-pair-kmd.lib',o/'ge8300-native-owner-core.lib',o/'display_adapter356.obj',*bdd,*libs,'/out:'+str(pkg/'ge8300-render.sys'),'/map:'+str(o/'ge8300-render359.map')],'link-kmd359.log')
inf=pkg/'ge8300-render.inf';bb=inf.read_bytes();enc='utf-16'if bb[:2]in (b'\xff\xfe',b'\xfe\xff')else'utf-8-sig';inf.write_text(bb.decode(enc).replace('0.0.3.58','0.0.3.59'),encoding=enc)
sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe');thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.sys'],'render359.sign.log')
transport=pkg/'display-transport';saved=t/'preserved-display-transport357';transport.rename(saved)
try:run(['C:/mt8167-hp1033/inputs/signing/bin/Inf2Cat.exe','/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime'],'render359.catalog.log')
finally:saved.rename(transport)
run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.cat'],'render359.cat-sign.log')
proof=[]
for n in ['ge8300-render.sys','ge8300-render.cat','ge8300-umd.dll','mt8167-dxgi-present356.exe','display-transport/mt8167-display-transport.sys','display-transport/mt8167-display-transport.cat']:
 log=n.replace('/','_')+'.signature359.log';run([sign,'verify','/pa',pkg/n],log);proof.append(log)
for cat,names in [('ge8300-render.cat',['ge8300-render.sys','ge8300-render.inf','ge8300-umd.dll','ge8300-mips.fw']),('display-transport/mt8167-display-transport.cat',['display-transport/mt8167-display-transport.sys','display-transport/mt8167-display-transport.inf'])]:
 for n in names:
  log=n.replace('/','_')+'.CAT359.log';run([sign,'verify','/pa','/c',pkg/cat,pkg/n],log);proof.append(log)
for x in old['outputs']:
 if x['name']not in {'ge8300-render.sys','ge8300-render.inf','ge8300-render.cat'}:assert sha(pkg/x['name'])==x['sha256']
proof+=['render359.sign.log','render359.catalog.log','render359.cat-sign.log'];(pkg/'proof359').mkdir();proofrows=[]
for n in proof:shutil.copyfile(o/'logs'/n,pkg/'proof359'/n);proofrows.append(dict(path='proof359/'+n,sha256=sha(pkg/'proof359'/n)))
m.update(schema='GPU359-start-gap-candidate-v1',package_source_commit=a.source_commit,kernel_binary_source_commit=a.source_commit,compile_source_commit=a.source_commit,packaging_source_commit=a.source_commit,runtime_build_id='GPU359-start-gap-v1',inf_driverver='11/09/2026,0.0.3.59',compiled359_units=selected,archives=archives,actual_include_closure=list(closures.values()),raw80_changed=delta,raw80_unchanged=79,display_extra_changed=['display_adapter356.obj','display_bdd.obj'],diagnostic_only=True,hardware_verified=False,rootfix_claim=False,controls_changed=False,capacity_changed=False,DIS0_357_bytes_preserved=True,baseline358_manifest_sha256=sha(b/'package358/signed-manifest.json'),verification_logs=proofrows)
m['outputs']=[dict(name=x['name'],sha256=sha(pkg/x['name']))for x in old['outputs']];m.pop('build_manifest_sha256',None);(o/'build-manifest359.json').write_text(json.dumps(m,indent=2)+'\n');shutil.copyfile(o/'build-manifest359.json',pkg/'start-gap359-build-manifest.json');m['build_manifest_sha256']=sha(pkg/'start-gap359-build-manifest.json');(pkg/'signed-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with zipfile.ZipFile(t/'outputs359.zip','w',zipfile.ZIP_DEFLATED)as z:
 for f in sorted(o.rglob('*')):
  if f.is_file():z.write(f,f.relative_to(o).as_posix())
print('359_3TU_SIGNED_CAPTURED_NO_HARDWARE_SUCCESS',sha(t/'outputs359.zip'))
