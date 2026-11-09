#!/usr/bin/env python3
"""Startup boundary candidate,3TUs only; immutable356r2 driver/UMD/FW origins retained."""
from pathlib import Path
import argparse,json,hashlib,subprocess,shutil,copy,re,zipfile
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True);p.add_argument('--source-commit',required=True);a=p.parse_args();t=a.task_root;r=t/'source';b=Path('C:/mt8167-present356/build-r2');o=t/'build';llvm=Path('C:/Program Files/LLVM/bin');sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();load=lambda p:json.loads(Path(p).read_text(encoding='utf-8-sig'))
def run(cmd,n=None):
 q=subprocess.run([str(x)for x in cmd],capture_output=True,text=True)
 if n:(o/'logs'/n).write_text(q.stdout+q.stderr)
 if q.returncode:raise RuntimeError((n or str(cmd[0]))+' '+q.stdout[-1000:]+q.stderr[-1500:])
 return q.stdout
assert run(['git','-C',r,'rev-parse','HEAD']).strip()==a.source_commit and not run(['git','-C',r,'status','--porcelain']).strip();run(['python',r/'tools/ci/verify-ge8300-fullsource.py'])
assert sha(b/'package356/signed-manifest.json')=='db305b820fd891d1b85da9908e5dac7b2ca187a33cc3ddf09eb72a9a97e2bfe0';old=load(b/'package356/signed-manifest.json')
for x in old['outputs']:assert sha(b/'package356'/x['name'])==x['sha256']
assert not o.exists();shutil.copytree(b,o);(o/'package356').rename(o/'package357');pkg=o/'package357';m=copy.deepcopy(old);changed=set(run(['git','-C',r,'diff','--name-only','c3fc95e615b49c243a05140a07d7c7a7a8e023cd','HEAD']).splitlines());selected=[];closure=[]
for u in m['compiled_units']:
 src=r/u['source'];flags=[str(x).replace('C:/mt8167-present356/source',r.as_posix()).replace('C:\\mt8167-present356\\source',str(r))for x in u['arguments']]
 if u['source'].endswith('/transport356.c') or u['source'].endswith('/windows_native_backing.c') or u['source'].endswith('/wddm-render/miniport.c'):
  pp=run([llvm/'clang.exe',*flags,'-H','-E',src,'-o','NUL'],u['object']+'.preprocess.log') if False else subprocess.run([str(llvm/'clang.exe'),*flags,'-H','-E',str(src),'-o','NUL'],capture_output=True,text=True)
  (o/'logs'/(u['object']+'.include.log')).write_text(pp.stderr);pp.check_returncode();local={u['source']}
  for line in pp.stderr.splitlines():
   h=re.match(r'^\.+ (.+)$',line)
   if h:
    try:local.add(Path(h[1].strip()).resolve().relative_to(r.resolve()).as_posix())
    except ValueError:pass
  assert 'drivers/gpu/mt8167/start_checkpoint357.h'in local or u['source'].endswith('/miniport.c')
  obj=o/({'native':'native-objects','kmd':'kmd-objects'}.get(u['role'],''))/u['object'];run([llvm/'clang.exe',*flags,'-c',src,'-o',obj],u['object']+'.compile.log')
  selected.append({'source':u['source'],'source_sha256':sha(src),'object':u['object'],'object_sha256':sha(obj),'role':u['role'],'arguments':flags,'actual_compile_source_commit':a.source_commit});closure.append({'source':u['source'],'local_closure':{n:sha(r/n)for n in sorted(local)},'compile_required':True});u.update(selected[-1])
assert len(selected)==3 and {u['role']for u in selected}=={'native','kmd','transport'}
# Recheck actual dependencies of every raw native/KMD member; any affected omitted TU is an error.
for u in old['actual_include_closure']:
 if u['role']not in ('native','kmd'):continue
 affected=set(u['local_closure'])&changed
 if affected:assert any(v['source']==u['source']for v in selected),'Missing include-closure TU '+u['source']
archives=[]
for arc in old['archives']:
 q=o/arc['file']
 if arc['role']in ['native','kmd']:
  d=o/(arc['role']+'-objects');members=[d/x['name']for x in arc['members']];run([llvm/'llvm-ar.exe','rcs',q,*members],arc['role']+'.archive.log');newarc=copy.deepcopy(arc);newarc['sha256']=sha(q)
  for x in newarc['members']:x['sha256']=sha(d/x['name'])
  archives.append(newarc)
 else:assert sha(q)==arc['sha256'];archives.append(copy.deepcopy(arc))
raw_old={x['name']:x['sha256']for ar in old['archives']if ar['role']in ('native','kmd')for x in ar['members']};raw_new={x['name']:x['sha256']for ar in archives if ar['role']in ('native','kmd')for x in ar['members']};delta=[n for n in raw_old if raw_old[n]!=raw_new[n]];assert len(raw_old)==len(raw_new)==80 and len(delta)==2
kit=Path('C:/mt8167-public-fullsource323/wdk81/Lib/winv6.3/km/arm');link=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/link.exe');assert sha(link)=='f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938'
flags=['/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000'];libs=[kit/(n+'.lib')for n in ['displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk']];bdd=[o/('display_'+n+'.obj')for n in ['bdd','bdd_dmm','bdd_util','bltfuncs','blthw','mt8167_brightness','unified_display','memory','ge8300_trace','ge8300_present']]
run([link,*flags,o/'ge8300-pair-kmd.lib',o/'ge8300-native-owner-core.lib',o/'display_adapter356.obj',*bdd,*libs,'/out:'+str(pkg/'ge8300-render.sys'),'/map:'+str(o/'ge8300-render.map')],'link-kmd357.log')
transport=pkg/'display-transport';run([link,*flags,o/'transport356.obj',o/'scanout_vidmm356.obj',*libs,'/out:'+str(transport/'mt8167-display-transport.sys'),'/map:'+str(o/'transport.map')],'link-transport357.log')
inf=pkg/'ge8300-render.inf';bb=inf.read_bytes();enc='utf-16'if bb[:2]in (b'\xff\xfe',b'\xfe\xff')else'utf-8-sig';inf.write_text(bb.decode(enc).replace('0.0.3.56','0.0.3.57'),encoding=enc);shutil.copyfile(r/'drivers/gpu/mt8167/display-transport/mt8167-display-transport.inf',transport/'mt8167-display-transport.inf')
sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe');thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
for q in [pkg/'ge8300-render.sys',transport/'mt8167-display-transport.sys']:run([sign,'sign','/fd','SHA256','/sha1',thumb,q],q.name+'.sign.log');run([sign,'verify','/pa',q],q.name+'.verify.log')
# Generate recursively first: parent Inf2Cat also recreates the child CAT.
run(['C:/mt8167-hp1033/inputs/signing/bin/Inf2Cat.exe','/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime'],'final-catalog-generation.log')
for d,cat in [(pkg,'ge8300-render.cat'),(transport,'mt8167-display-transport.cat')]:
 run([sign,'sign','/fd','SHA256','/sha1',thumb,d/cat],cat+'.sign.log')
for d,cat in [(pkg,'ge8300-render.cat'),(transport,'mt8167-display-transport.cat')]:
 run([sign,'verify','/pa',d/cat],cat+'.final-verify.log')
 for name in (['mt8167-display-transport.sys','mt8167-display-transport.inf'] if d==transport else ['ge8300-render.sys','ge8300-render.inf','ge8300-umd.dll','ge8300-mips.fw']):run([sign,'verify','/pa','/c',d/cat,d/name],name+'.CAT.log')
changed_out={'ge8300-render.sys','ge8300-render.inf','ge8300-render.cat','display-transport/mt8167-display-transport.sys','display-transport/mt8167-display-transport.inf','display-transport/mt8167-display-transport.cat'}
for x in old['outputs']:
 if x['name']not in changed_out:assert sha(pkg/x['name'])==x['sha256']
m.update(schema='GPU357-bounded-start-predicates-candidate-v1',package_source_commit=a.source_commit,kernel_binary_source_commit='14ac163b9e579d91f72a99e321da292412bf1f4c',native_owner_source_commit='14ac163b9e579d91f72a99e321da292412bf1f4c',display_transport_source_commit='14ac163b9e579d91f72a99e321da292412bf1f4c',link_recipe_source_commit=a.source_commit,runtime_build_id='GPU357-start-predicates-v1',inf_driverver='11/09/2026,0.0.3.57',archives=archives,actual_include_closure=closure,compiled357_units=selected,raw80_changed=delta,raw80_unchanged=78,rootfix_claim=False,hardware_verified=False,extra_ZwFlushKey=0,controls_changed=False,capacity_changed=False,matched_boot_sha256='0943ab8a6008942d9701140542da6ecaa902f750bf34b864bb6a66808c08df42',baseline356r2_manifest_sha256=sha(b/'package356/signed-manifest.json'),source_capture_helper_fix_retained=True)
m['outputs']=[dict(name=x['name'],sha256=sha(pkg/x['name']))for x in old['outputs']];m.pop('build_manifest_sha256',None);(o/'build-manifest357.json').write_text(json.dumps(m,indent=2)+'\n');shutil.copyfile(o/'build-manifest357.json',pkg/'start357-build-manifest.json');m['build_manifest_sha256']=sha(pkg/'start357-build-manifest.json');(pkg/'signed-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with zipfile.ZipFile(t/'outputs357.zip','w',zipfile.ZIP_DEFLATED)as z:
 for q in o.rglob('*'):
  if q.is_file():z.write(q,q.relative_to(o).as_posix())
print('SIGNED357_BOUNDARIES_ONLY_3TU_RAW80CHANGE2_HW0',sha(pkg/'signed-manifest.json'))
