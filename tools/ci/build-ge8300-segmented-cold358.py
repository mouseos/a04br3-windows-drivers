#!/usr/bin/env python3
"""Fixed-source358, two C TUs from the final signed357 cache; no device operations."""
from pathlib import Path
import argparse,json,hashlib,subprocess,shutil,copy,re,zipfile
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True);p.add_argument('--source-commit',required=True);a=p.parse_args();t=a.task_root;r=t/'source';b=Path('C:/mt8167-start357/build');o=t/'build';llvm=Path('C:/Program Files/LLVM/bin');sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();load=lambda p:json.loads(Path(p).read_text(encoding='utf-8-sig'))
def run(cmd,name=None):
 q=subprocess.run([str(x)for x in cmd],capture_output=True,text=True)
 if name:(o/'logs'/name).write_text(q.stdout+q.stderr)
 if q.returncode:raise RuntimeError((name or str(cmd[0]))+' '+q.stdout[-1000:]+q.stderr[-1500:])
 return q.stdout
assert run(['git','-C',r,'rev-parse','HEAD']).strip()==a.source_commit and not run(['git','-C',r,'status','--porcelain']).strip()
run(['python',r/'tools/ci/verify-ge8300-fullsource.py'])
assert sha(b/'package357/signed-manifest.json')=='60c14c5f00f6cdcdead36de23ac9c4da5829aa227ecc21bf1b335d7af1bd0e90';old=load(b/'package357/signed-manifest.json')
for x in old['outputs']:assert sha(b/'package357'/x['name'])==x['sha256']
for ar in old['archives']:
 assert sha(b/ar['file'])==ar['sha256']
 for x in ar['members']:
  if ar['role'] in ('native','kmd'):assert sha(b/(ar['role']+'-objects')/x['name'])==x['sha256']
assert not o.exists();shutil.copytree(b,o);(o/'package357').rename(o/'package358');pkg=o/'package358';m=copy.deepcopy(old)
changed=set(run(['git','-C',r,'diff','--name-only','a27b8cc9b477ba364f7dd1d0a58665202fd1472f','HEAD']).splitlines())
expected={'drivers/gpu/mt8167/vendor_m1_11/render_owner/windows_native_job.c','drivers/gpu/mt8167/wddm-render/miniport.c'}
assert {x for x in changed if x.startswith('drivers/')}==expected,'Production scope/header change'
base=load(Path('C:/mt8167-present356/build-r2/package356/signed-manifest.json'))
closures={u['source']:copy.deepcopy(u)for u in base['actual_include_closure']}
closures.update({u['source']:copy.deepcopy(u)for u in old['actual_include_closure']})
for u in closures.values():
 if u.get('role')in ('native','kmd'):
  affected=set(u['local_closure'])&expected
  assert not affected or u['source']in expected,'Omitted affected TU '+u['source']
selected=[]
for u in m['compiled_units']:
 if u['source']not in expected:continue
 src=r/u['source'];flags=[]
 for arg in u['arguments']:
  for oldroot in ['C:/mt8167-present356/source','C:\\mt8167-present356\\source','C:/mt8167-start357/source','C:\\mt8167-start357\\source']:arg=str(arg).replace(oldroot,r.as_posix())
  flags.append(arg)
 pp=subprocess.run([str(llvm/'clang.exe'),*flags,'-H','-E',str(src),'-o','NUL'],capture_output=True,text=True);(o/'logs'/(u['object']+'.include358.log')).write_text(pp.stderr);pp.check_returncode();local={u['source']}
 for line in pp.stderr.splitlines():
  h=re.match(r'^\.+ (.+)$',line)
  if h:
   try:local.add(Path(h[1].strip()).resolve().relative_to(r.resolve()).as_posix())
   except ValueError:pass
 obj=o/(u['role']+'-objects')/u['object'];run([llvm/'clang.exe',*flags,'-c',src,'-o',obj],u['object']+'.compile358.log')
 entry=dict(source=u['source'],source_sha256=sha(src),object=u['object'],object_sha256=sha(obj),role=u['role'],arguments=flags,actual_compile_source_commit=a.source_commit);selected.append(entry);u.update(entry);closures[u['source']]=dict(source=u['source'],role=u['role'],local_closure={n:sha(r/n)for n in sorted(local)},compile_required=True)
assert len(selected)==2
archives=[]
for arc in old['archives']:
 q=o/arc['file'];newarc=copy.deepcopy(arc)
 if arc['role']in ('native','kmd'):
  d=o/(arc['role']+'-objects');run([llvm/'llvm-ar.exe','rcs',q,*[d/x['name']for x in arc['members']]],arc['role']+'.archive358.log');newarc['sha256']=sha(q)
  for x in newarc['members']:x['sha256']=sha(d/x['name'])
 else:assert sha(q)==arc['sha256']
 archives.append(newarc)
raw_old={x['name']:x['sha256']for ar in old['archives']if ar['role']in ('native','kmd')for x in ar['members']};raw_new={x['name']:x['sha256']for ar in archives if ar['role']in ('native','kmd')for x in ar['members']};delta=[n for n in raw_old if raw_old[n]!=raw_new[n]];assert len(raw_old)==len(raw_new)==80 and len(delta)==2
kit=Path('C:/mt8167-public-fullsource323/wdk81/Lib/winv6.3/km/arm');link=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/link.exe');assert sha(link)=='f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938'
flags=['/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000'];libs=[kit/(n+'.lib')for n in ['displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk']];bdd=[o/('display_'+n+'.obj')for n in ['bdd','bdd_dmm','bdd_util','bltfuncs','blthw','mt8167_brightness','unified_display','memory','ge8300_trace','ge8300_present']]
run([link,*flags,o/'ge8300-pair-kmd.lib',o/'ge8300-native-owner-core.lib',o/'display_adapter356.obj',*bdd,*libs,'/out:'+str(pkg/'ge8300-render.sys'),'/map:'+str(o/'ge8300-render358.map')],'link-kmd358.log')
inf=pkg/'ge8300-render.inf';bb=inf.read_bytes();enc='utf-16'if bb[:2]in (b'\xff\xfe',b'\xfe\xff')else'utf-8-sig';inf.write_text(bb.decode(enc).replace('0.0.3.57','0.0.3.58'),encoding=enc)
sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe');thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.sys'],'render358.sign.log')
# Isolate unchanged child package: recursive Inf2Cat must not regenerate357 DIS0 CAT.
transport=pkg/'display-transport';saved=t/'preserved-display-transport357';transport.rename(saved)
try:run(['C:/mt8167-hp1033/inputs/signing/bin/Inf2Cat.exe','/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime'],'render358.catalog.log')
finally:saved.rename(transport)
run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.cat'],'render358.cat-sign.log')
for d,cat,names in [(pkg,'ge8300-render.cat',['ge8300-render.sys','ge8300-render.inf','ge8300-umd.dll','ge8300-mips.fw']),(transport,'mt8167-display-transport.cat',['mt8167-display-transport.sys','mt8167-display-transport.inf'])]:
 run([sign,'verify','/pa',d/cat],cat+'.verify358.log')
 for name in names:run([sign,'verify','/pa','/c',d/cat,d/name],name+'.CAT358.log')
for name in ['ge8300-render.sys','ge8300-render.cat','ge8300-umd.dll','mt8167-dxgi-present356.exe','display-transport/mt8167-display-transport.sys','display-transport/mt8167-display-transport.cat']:run([sign,'verify','/pa',pkg/name],Path(name).name+'.signature358.log')
changed_out={'ge8300-render.sys','ge8300-render.inf','ge8300-render.cat'}
for x in old['outputs']:
 if x['name']not in changed_out:assert sha(pkg/x['name'])==x['sha256']
m.pop('packaging_only_recovery',None);m.update(schema='GPU358-segmented-cold-candidate-v1',package_source_commit=a.source_commit,compile_source_commit=a.source_commit,kernel_binary_source_commit=a.source_commit,native_owner_source_commit=a.source_commit,link_recipe_source_commit=a.source_commit,packaging_source_commit=a.source_commit,runtime_build_id='GPU358-segmented-cold-v1',inf_driverver='11/09/2026,0.0.3.58',archives=archives,actual_include_closure=list(closures.values()),compiled358_units=selected,raw80_changed=delta,raw80_unchanged=78,rootfix_claim=False,hardware_verified=False,controls_changed=False,capacity_changed=False,DIS0_357_bytes_preserved=True,baseline357_manifest_sha256=sha(b/'package357/signed-manifest.json'))
m['outputs']=[dict(name=x['name'],sha256=sha(pkg/x['name']))for x in old['outputs']];m.pop('build_manifest_sha256',None);(o/'build-manifest358.json').write_text(json.dumps(m,indent=2)+'\n');shutil.copyfile(o/'build-manifest358.json',pkg/'segmented358-build-manifest.json');m['build_manifest_sha256']=sha(pkg/'segmented358-build-manifest.json');(pkg/'signed-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with zipfile.ZipFile(t/'outputs358.zip','w',zipfile.ZIP_DEFLATED)as z:
 for q in o.rglob('*'):
  if q.is_file():z.write(q,q.relative_to(o).as_posix())
print('SIGNED358_2TU_RAW80CHANGE2_DIS0357_UNCHANGED_HW0',sha(pkg/'signed-manifest.json'))
