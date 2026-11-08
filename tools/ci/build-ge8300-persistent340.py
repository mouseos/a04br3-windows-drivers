#!/usr/bin/env python3
"""Incremental persistent-copy runtime from sealed339; preserve unrelated objects and UMD/PCO/FW/client bytes."""
import argparse,hashlib,json,ntpath,shutil,subprocess,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True);p.add_argument('--source-commit',required=True);a=p.parse_args()
t=a.task_root;root=t/'source';base=t/'baseline';pkgbase=base/'package339';out=t/'build';pkg=out/'package340'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def pin(p,h):
 if sha(p)!=h:raise ValueError('Pin mismatch: '+str(p))
def run(cmd):subprocess.run([str(x) for x in cmd],check=True)
def capture(cmd):return subprocess.check_output([str(x) for x in cmd],text=True).strip()
def relocate(x):
 x=str(x).replace(chr(92)*2,chr(92))
 for prefix in ['C:/mt8167-copy-integration/source/drivers','C:/mt8167-moduleidentity339/source/drivers']:
  x=x.replace(prefix.replace('/',chr(92)),str(root/'drivers'))
 return x
if out.exists():raise ValueError('New output required; previous results retained')
pin(pkgbase/'signed-manifest.json','f3d3ce288dbc8b453fea220276eee5577344c0704c0895930853f151fd01dfcf')
m=load(pkgbase/'signed-manifest.json')
for x in m['outputs']+m['retained_provenance']:pin(pkgbase/x['name'],x['sha256'])
for x in m['archives']:
 if x['role'] in ['kmd','native']:pin(base/x['file'],x['sha256'])
out.mkdir();pkg.mkdir();(out/'objects-before').mkdir();(out/'objects').mkdir()
llvm=Path('C:/Program Files/LLVM/bin');clang=llvm/'clang.exe';ar=llvm/'llvm-ar.exe'
kmd=next(x for x in m['archives'] if x['role']=='kmd');native=next(x for x in m['archives'] if x['role']=='native')
names=[ntpath.basename(n) for n in capture([ar,'t',base/kmd['file']]).splitlines()]
if len(names)!=13 or set(names)!={x['name'] for x in kmd['members']}:raise ValueError('Exact337 KMD13 member map required')
run([ar,'x','--output='+str(out/'objects-before'),base/kmd['file']])
for x in kmd['members']:pin(out/'objects-before'/x['name'],x['sha256']);shutil.copyfile(out/'objects-before'/x['name'],out/'objects'/x['name'])
u=next(x for x in load(pkgbase/'copy337-build-manifest.json')['compiled_units'] if x['source']=='drivers/gpu/mt8167/wddm-render/miniport.c');flags=[relocate(x) for x in u['arguments']]
obj=out/'objects'/u['object'];src=root/u['source'];expected=load(t/'input-manifest.json')
for x in expected:pin(t/x['file'],x['sha256'])
run([clang,*flags,'-c',src,'-o',obj]);units=[{'source':u['source'],'source_sha256':sha(src),'object':u['object'],'object_sha256':sha(obj),'arguments':flags,'role':'kmd'}]
rows=[]
for x in kmd['members']:
 h=sha(out/'objects'/x['name']);changed=x['name']==u['object']
 if not changed and h!=x['sha256']:raise ValueError('Unrelated337 object changed')
 rows.append({'name':x['name'],'sha256':h,'before_sha256':x['sha256'],'compiled':changed})
run([llvm/'llvm-lib.exe','/machine:arm','/out:'+str(out/kmd['file']),*[out/'objects'/n for n in reversed(names)]])
if [ntpath.basename(n) for n in capture([ar,'t',out/kmd['file']]).splitlines()]!=names:raise ValueError('KMD archive member order changed')

# Only four changed native TUs; retain the other63 exact object hashes/order.
(out/'native-before').mkdir();(out/'native-objects').mkdir()
native_names=[ntpath.basename(n) for n in capture([ar,'t',base/native['file']]).splitlines()]
if len(native_names)!=67 or set(native_names)!={x['name'] for x in native['members']}:raise ValueError('Native67 member closure')
run([ar,'x','--output='+str(out/'native-before'),base/native['file']])
for x in native['members']:pin(out/'native-before'/x['name'],x['sha256']);shutil.copyfile(out/'native-before'/x['name'],out/'native-objects'/x['name'])
changed_native={'native_owner_factory.c','native_owner_factory_runtime.c','windows_native_copy_storage.c','windows_native_copy_transfer.c'}
for nu in load(pkgbase/'copy337-build-manifest.json')['compiled_units']:
 if nu['role']!='native' or ntpath.basename(nu['source']) not in changed_native:continue
 args=[relocate(x) for x in nu['arguments']]
 ns=root/nu['source'];no=out/'native-objects'/nu['object'];run([clang,*args,'-c',ns,'-o',no])
 units.append({'source':nu['source'],'source_sha256':sha(ns),'object':nu['object'],'object_sha256':sha(no),'arguments':args,'role':'native'})
if len(units)!=5:raise ValueError('Exactly four native plus one miniport compiled')
native_rows=[]
changed_objects={x['object'] for x in units if x['role']=='native'}
for x in native['members']:
 h=sha(out/'native-objects'/x['name']);changed=x['name'] in changed_objects
 if not changed and h!=x['sha256']:raise ValueError('Unrelated native object changed')
 native_rows.append({'name':x['name'],'sha256':h,'before_sha256':x['sha256'],'compiled':changed})
run([llvm/'llvm-lib.exe','/machine:arm','/out:'+str(out/native['file']),*[out/'native-objects'/n for n in reversed(native_names)]])
if [ntpath.basename(n) for n in capture([ar,'t',out/native['file']]).splitlines()]!=native_names:raise ValueError('Native archive member order changed')

link=Path(load(t/'host-tools.json')['link']);sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe');cat=Path('C:/mt8167-copy-integration/catalog-tools/Inf2Cat.exe')
kit=Path('C:/mt8167-public-fullsource323/wdk81/Lib/winv6.3/km/arm');libs=[kit/(x+'.lib') for x in ['displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk']]
run([link,'/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/base:0x400000',out/kmd['file'],out/native['file'],*libs,'/out:'+str(out/'ge8300-render.sys'),'/map:'+str(out/'ge8300-render.map')])
thumb='BCBF6C456C6F0DD0818434763B189488FBF13570';run([sign,'sign','/fd','SHA256','/sha1',thumb,out/'ge8300-render.sys']);run([sign,'verify','/pa',out/'ge8300-render.sys'])
for f in pkgbase.iterdir():
 if f.is_file() and f.name not in ['ge8300-render.cat','ge8300-render.sys','ge8300-render.inf','signed-manifest.json']:shutil.copyfile(f,pkg/f.name)
shutil.copyfile(pkgbase/'signed-manifest.json',pkg/'baseline339-signed-manifest.json');shutil.copyfile(out/'ge8300-render.sys',pkg/'ge8300-render.sys')
b=(pkgbase/'ge8300-render.inf').read_bytes();encoding='utf-16' if b[:2] in [b'\xff\xfe',b'\xfe\xff'] else 'utf-8-sig';inf=b.decode(encoding);old='DriverVer=11/07/2026,0.0.3.39';new='DriverVer=11/07/2026,0.0.3.40'
if inf.count(old)!=1:raise ValueError('Exact337 INF DriverVer required')
(pkg/'ge8300-render.inf').write_text(inf.replace(old,new),encoding=encoding)
run([cat,'/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime']);run([sign,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.cat'])
for n in ['ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw']:run([sign,'verify','/pa','/c',pkg/'ge8300-render.cat',pkg/n])
for x in m['outputs']:
 if x['name'] not in ['ge8300-render.inf','ge8300-render.cat','ge8300-render.sys']:pin(pkg/x['name'],x['sha256'])
archives=[]
for x in m['archives']:
 y=dict(x)
 if x['role']=='kmd':y.update(sha256=sha(out/kmd['file']),members=rows)
 if x['role']=='native':y.update(sha256=sha(out/native['file']),members=native_rows)
 archives.append(y)
newm=dict(m);newm.update(schema='persistentcopy340-independent-sources-v1',module_identity_evidence_only=False,module_identity_record_bytes=144,module_identity_key='Parameters/NativeCheckpoint/ModuleIdentityV1',module_identity_runtime_source_base='acc343370cca6fbda412ebf3a535c0a583e37cbd',package_source_commit=a.source_commit,kernel_binary_source_commit=a.source_commit,native_owner_source_commit=a.source_commit,native_owner_changed=True,umd_changed=False,compiler_changed=False,PCO_core_rebuilt=False,SDK_rebuilt=False,baseline_manifest_file='baseline339-signed-manifest.json',baseline_manifest_sha256=sha(pkg/'baseline339-signed-manifest.json'),compiled_units=units,archives=archives,inf_driverver='11/07/2026,0.0.3.40',build_manifest_file='persistentcopy340-build-manifest.json',copy_failure_gate=True,failed_Render_refs_retained=True,hardware_verified=False,runtime_verified=False,root_reset_fix_proven=False)
newm['outputs']=[{'name':x['name'],'sha256':sha(pkg/x['name'])} for x in m['outputs']]
newm['retained_provenance']=[{'name':f.name,'sha256':sha(f)} for f in pkg.iterdir() if f.is_file() and f.name not in {x['name'] for x in newm['outputs']}]
# Build receipt before the signed manifest avoids recursive manifest hashing.
(out/'build-manifest.json').write_text(json.dumps(newm,indent=2)+'\n');shutil.copyfile(out/'build-manifest.json',pkg/'persistentcopy340-build-manifest.json');newm['build_manifest_sha256']=sha(pkg/'persistentcopy340-build-manifest.json')
(pkg/'signed-manifest.json').write_text(json.dumps(newm,indent=2)+'\n');(out/'imports.txt').write_text(capture([llvm/'llvm-readobj.exe','--coff-imports',pkg/'ge8300-render.sys']))
with zipfile.ZipFile(t/'outputs.zip','w',zipfile.ZIP_DEFLATED) as z:
 for f in out.rglob('*'):
  if f.is_file():z.write(f,f.relative_to(out).as_posix())
print('PERSISTENTCOPY340_FIVE_TU_SIGNED',sha(pkg/'signed-manifest.json'),sha(pkg/'ge8300-render.sys'))
