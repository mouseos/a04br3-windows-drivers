#!/usr/bin/env python3
"""Build the unified356 candidate from a fixed tree and verified355 caches.
No device access. A signature/CPU build never proves a displayed GPU frame.
"""
import argparse,hashlib,json,subprocess,re,shutil,ntpath,struct,zipfile
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--task-root',type=Path,required=True);p.add_argument('--source-commit',required=True);p.add_argument('--resume-stage-sha256');a=p.parse_args()
t=a.task_root;r=t/'source';o=t/'build';base=Path('C:/mt8167-present355-kernel/build');ub=Path('C:/mt8167-present355/build-r2');cache=Path('C:/mt8167-present355/cache');llvm=Path('C:/Program Files/LLVM/bin');kit=Path('C:/mt8167-public-fullsource323/wdk81');link=Path('C:/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/MSVC/14.29.30133/bin/Hostx64/arm/link.exe');cl=link.with_name('cl.exe')
def sha(f):return hashlib.sha256(Path(f).read_bytes()).hexdigest()
def load(f):return json.loads(Path(f).read_text(encoding='utf-8-sig'))
def write(f,v):Path(f).write_text(json.dumps(v,indent=2)+'\n')
def pin(f,h):
 if sha(f)!=h:raise ValueError('SHA mismatch '+str(f))
def run(args,log=None):
 result=subprocess.run([str(x)for x in args],capture_output=True,text=True)
 if log:Path(log).write_text(result.stdout+result.stderr)
 if result.returncode:raise RuntimeError(str(args[0])+' failed '+(str(log)if log else result.stdout[-1800:]+result.stderr[-1800:]))
 return result.stdout
if run(['git','-C',r,'rev-parse','HEAD']).strip()!=a.source_commit or run(['git','-C',r,'status','--porcelain']).strip():raise ValueError('Fixed clean source required')
run(['python',r/'tools/ci/verify-ge8300-fullsource.py'])
pin(link,'f8b2e9c5341419525f915b61b87d1bc8aa307adca6e1e022071175912e965938')
pin(base/'package355/signed-manifest.json','43f49646379a79900e78279ed34484c71a59f9b02a78de986aab2410485a08a0')
m=load(base/'package355/signed-manifest.json')
for x in m['outputs']:pin(base/'package355'/x['name'],x['sha256'])
stage=None
if o.exists():
 if not a.resume_stage_sha256:raise ValueError('Existing build requires the pinned failed-stage receipt')
 pin(t/'failed-link-inputs.json',a.resume_stage_sha256);stage=load(t/'failed-link-inputs.json')
 if stage['source_commit']!='ac16aa83abf3e6ebeb49ff1a4dbcc5be620c8b98':raise ValueError('Exact failed source required')
 for n,h in stage['objects'].items():pin(o/n,h)
else:o.mkdir()
(o/'deps').mkdir(exist_ok=True);(o/'logs').mkdir(exist_ok=True);pkg=o/'package356';pkg.mkdir(exist_ok=True)
changed=set(run(['git','-C',r,'diff','--name-only','39492c1084ab7e9b41a0efc8a3f7221bd94c08d5','HEAD']).splitlines())
def fix(x):
 x=str(x).replace('\\','/')
 while '//'in x:x=x.replace('//','/')
 x=re.sub(r'C:/mt8167-[^\s]+?/(?:source|src)/(?=drivers|third_party)',r.as_posix()+'/',x,flags=re.I)
 x=re.sub(r'C:/mt8167-[^\s]+?/(?:source/)?build/(?:copy336/)?generated','C:/mt8167-present355-kernel/build/generated',x,flags=re.I)
 return x
rows=load(t/'all-units.json');uf=load(ub/'build-manifest.json')['compiled_units'][0]['arguments']
um_archive=next(x for x in m['archives']if x['role']=='umd')
for member in um_archive['members']:
 name=member['name'][:-4]
 if name.startswith('umd_'):src='umd/'+name[4:]+'.c'
 elif name.startswith('frontend_'):src='frontend/'+name[9:]+'.c'
 elif name.startswith('compiler_'):src='compiler/'+name[9:]+'.c'
 else:src=name+'.c'
 f=list(uf)
 if name=='umd_typed_state':f=[x for x in f if x not in ['-D_D3D10_CONSTANTS','-D_D3D10_1_CONSTANTS']]
 if name=='compiler_pco_texture2d':
  plan=load(r/'third_party/mesa-pco/source-plan.json');f=next(x['arguments']for x in plan['units']if x['source']=='drivers/gpu/mt8167/wddm-render/compiler/pco_sm2.c')
  inc=Path('C:/Program Files (x86)/Windows Kits/10/Include');ucrt=next(x/'ucrt'for x in sorted(inc.iterdir(),reverse=True)if(x/'ucrt/stddef.h').exists())
  macros={'REPO':r.as_posix(),'VC_INCLUDE':link.parents[3].as_posix()+'/include','UCRT_INCLUDE':ucrt.as_posix(),'SDK_SHARED':'C:/mt8167-minimal-service-host/cache/kit81/Include/shared','SDK_UM':'C:/mt8167-minimal-service-host/cache/kit81/Include/um'}
  for k,v in macros.items():f=[x.replace('${'+k+'}',v)for x in f]
  if any('${'in x for x in f):raise ValueError('Unresolved compiler adapter include')
 rows.append(dict(source='drivers/gpu/mt8167/wddm-render/'+src,object=member['name'],role='umd',arguments=f))
for n in ['entry','typed_entry']:
 f=list(uf)+(['-DGEO_LINKED_REAL_FRONTEND=1']if n=='entry'else['-DGEO_TYPED_LIMITED_DEVELOPMENT=1','-DGEO_TYPED_NATIVE_PIPELINE10=1'])
 rows.append(dict(source='drivers/gpu/mt8167/wddm-render/umd/'+n+'.c',object='umd_'+n+'.obj',role='umd-entry',arguments=f))
for src,role,name in [('wddm-render/display_adapter356.c','bridge','display_adapter356.obj'),('display-transport/transport356.c','transport','transport356.obj'),('vendor_m1_11/windows_shared_scanout.c','transport','scanout_vidmm356.obj')]:
 f=list(rows[0]['arguments'])
 if 'windows_shared_scanout'in src:f+=['-DA04_SHARED_SCANOUT_VIDMM_ONLY=1','-ffunction-sections']
 rows.append(dict(source='drivers/gpu/mt8167/'+src,object=name,role=role,arguments=f))
archives=[];objects={};records=[];closure=[]
for arc in m['archives']:
 if arc['role']not in ['native','kmd','umd']:archives.append(arc);continue
 path=(ub/'ge8300-native-umd.lib')if arc['role']=='umd'else base/arc['file'];pin(path,arc['sha256']);d=o/(arc['role']+'-objects');d.mkdir(exist_ok=True)
 if not stage:run([llvm/'llvm-ar.exe','x','--output='+str(d),path])
 for x in arc['members']:
  if not stage:pin(d/x['name'],x['sha256'])
  objects[(arc['role'],x['name'])]=d/x['name']
prior={x['object']:x for x in load(t/'compile-check/result.json')if x.get('sha256')}
for u in rows:
 src=r/u['source'];args=[fix(x)for x in u['arguments']]+['-DA04_GE8300_UNIFIED_DISPLAY356=1'];name=u['object'];role=u['role'];obj=objects.get((role,name),o/name)
 if src.name=='mdl_pin.c':
  # SEH TU includes only frontend_memory.h, has no frame/layout use. Use the
  # genuine MSVC preprocessor; reuse is still content-closure verified.
  pp=[cl,'/nologo','/EP','/showIncludes','/TC']+['/D'+x[2:]for x in args if x.startswith('-D')]+['/I'+x[2:]for x in args if x.startswith('-I')and'/llvm/'not in x.lower()]+['/I'+str(link.parents[3]/'include'),src]
  output=run(pp,o/'deps'/name);text=output
 else:
  dep=subprocess.run([str(llvm/'clang.exe'),*args,'-H','-E',str(src),'-o','NUL'],capture_output=True,text=True);text=dep.stderr;(o/'deps'/name).write_text(text)
  if dep.returncode:raise RuntimeError('Preprocess '+u['source']+' '+text[-1000:])
 local={u['source']}
 for line in text.splitlines():
  hit=re.match(r'^\.+ (.+)$',line)or re.match(r'^Note: including file:\s*(.+)$',line)
  if hit:
   try:local.add(Path(hit[1].strip()).resolve().relative_to(r.resolve()).as_posix())
   except ValueError:pass
 hashes={n:sha(r/n)for n in sorted(local)};affected=sorted(local&changed)
 needed=bool(affected)or role in ['bridge','transport','umd-entry'];recovered=False
 if needed:
  from_stage=bool(stage and obj.exists()and stage['objects'].get(obj.relative_to(o).as_posix())==sha(obj)and all(stage['source_files'].get(n)==h for n,h in hashes.items()))
  before=prior.get(name)
  if from_stage:recovered=True
  elif before and before['arguments']==args and before['local_closure']==hashes:
   pin(t/'compile-check'/name,before['sha256']);shutil.copyfile(t/'compile-check'/name,obj);recovered=True
  else:run([llvm/'clang.exe',*args,'-c',src,'-o',obj],o/'logs'/(name+'.log'))
  records.append(dict(source=u['source'],object=name,role=role,arguments=args,source_sha256=sha(src),object_sha256=sha(obj),recovered_identical_compile_snapshot=recovered,recovered_stage_source_commit=stage['source_commit']if from_stage else None))
 elif not obj.exists():raise ValueError('Missing retained object '+name)
 closure.append(dict(source=u['source'],object=name,role=role,local_closure=hashes,changed_includes=affected,compile_required=needed))
 objects[(role,name)]=obj
for arc in m['archives']:
 if arc['role']not in ['native','kmd','umd']:continue
 role=arc['role'];members=[]
 for x in arc['members']:
  q=objects[(role,x['name'])];h=sha(q);compiled=any(u['object']==x['name']and u['role']==role for u in records)
  if not compiled:pin(q,x['sha256'])
  members.append(dict(name=x['name'],sha256=h,before_sha256=x['sha256'],compiled=compiled))
 path=o/arc['file'];run([llvm/'llvm-lib.exe','/machine:arm','/out:'+str(path),*[objects[(role,x['name'])]for x in reversed(arc['members'])]])
 names=[ntpath.basename(n)for n in run([llvm/'llvm-ar.exe','t',path]).splitlines()]
 if names!=[x['name']for x in arc['members']]:raise ValueError('Archive order '+role)
 archives.append(dict(role=role,file=path.name,sha256=sha(path),members=members))
# Actual DOD1.7 sources under the same miniport. No bdd_ddi DriverEntry/CDO.
uc=next(x/'ucrt'for x in sorted(Path('C:/Program Files (x86)/Windows Kits/10/Include').iterdir(),reverse=True)if(x/'ucrt/stddef.h').exists())
bflags=['/nologo','/c','/kernel','/Gy','/GS-','/Zl','/D_KERNEL_MODE','/D_ARM_','/DARM','/DNTDDI_VERSION=0x06030000','/D_WIN32_WINNT=0x0603','/DDXGKDDI_INTERFACE_VERSION=0x4002','/DA04_MT8167_DISPLAY_BRIGHTNESS=1','/DA04_GE8300_UNIFIED_DISPLAY356=1','/DA04_GE8300_WDDM_DIRECT_EXPERIMENT=0','/DA04_GE8300_WDDM_SHARED_AUTOSTART=0','/DA04_GE8300_WDDM_VENDOR_TILE=0','/DA04_GE8300_WDDM_COPY=0','/I'+str(kit/'Include/km'),'/I'+str(kit/'Include/shared'),'/IC:/mt8167-minimal-service-host/cache/kit81/Include/um','/I'+str(link.parents[3]/'include'),'/I'+str(uc),'/I'+str(r/'drivers/gpu/mt8167/wddm-display')]
bdd=[]
for n in ['bdd','bdd_dmm','bdd_util','bltfuncs','blthw','mt8167_brightness','unified_display','memory','ge8300_trace','ge8300_present']:
 src=r/'drivers/gpu/mt8167/wddm-display'/(n+'.cxx');obj=o/('display_'+n+'.obj');run([cl,*bflags,'/showIncludes','/Fo'+str(obj),src],o/'logs'/('display_'+n+'.log'));bdd.append(obj);records.append(dict(source=src.relative_to(r).as_posix(),source_sha256=sha(src),object=obj.name,object_sha256=sha(obj),role='display',arguments=bflags))
kl=kit/'Lib/winv6.3/km/arm';kernel=['/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/largeaddressaware','/tsaware:no','/base:0x400000']
libs=[kl/(n+'.lib')for n in ['displib','ntoskrnl','hal','libcntpr','bufferoverflowfastfailk']]
run([link,*kernel,o/'ge8300-pair-kmd.lib',o/'ge8300-native-owner-core.lib',o/'display_adapter356.obj',*bdd,*libs,'/out:'+str(pkg/'ge8300-render.sys'),'/map:'+str(o/'ge8300-render.map')],o/'logs/link-kmd.log')
transport=pkg/'display-transport';transport.mkdir(exist_ok=True)
run([link,*kernel,o/'transport356.obj',o/'scanout_vidmm356.obj',*libs,'/out:'+str(transport/'mt8167-display-transport.sys'),'/map:'+str(o/'transport.map')],o/'logs/link-transport.log')
# PCO375 and all other compiler archives are reused and pinned, not rebuilt.
cm=load(cache/'compiler/link-manifest.json');ul=[]
for x in cm['inputs']:
 if x['role']=='verified standard UMD module':continue
 n=ntpath.basename(x['path']);q=(cache/'baseline'/n)if n=='ge8300-pco-arm32.lib'else cache/'compiler'/n;pin(q,x['sha256']);ul.append(q)
if len(ul)!=10:raise ValueError('Compiler closure10')
run([llvm/'lld-link.exe','/dll','/machine:arm','/subsystem:windows,6.03','/entry:_DllMainCRTStartup','/nodefaultlib',objects[('umd-entry','umd_entry.obj')],objects[('umd-entry','umd_typed_entry.obj')],o/'ge8300-native-umd.lib',cache/'pair/pco_pair.obj',*ul,'/out:'+str(pkg/'ge8300-umd.dll'),'/map:'+str(o/'ge8300-umd.map')],o/'logs/link-umd.log')
client=r/'drivers/gpu/mt8167/wddm-render/testclient';cf=[fix(x)for x in uf if x not in ['-D_D3D10_CONSTANTS','-D_D3D10_1_CONSTANTS']]
for name in ['dxgi-present356','memory']:run([llvm/'clang.exe',*cf,'-c',client/(name+'.c'),'-o',o/(name+'.obj')],o/'logs'/('client-'+name+'.log'))
ulib=Path('C:/Program Files (x86)/Windows Kits/10/Lib/10.0.17763.0/um/arm')
run([link,'/nologo','/machine:arm','/subsystem:console,6.03','/entry:mainCRTStartup','/nodefaultlib','/largeaddressaware','/tsaware:no',o/'dxgi-present356.obj',o/'memory.obj',*[ulib/(n+'.lib')for n in ['kernel32','user32','d3d11','dxgi','setupapi'] ],'/out:'+str(pkg/'mt8167-dxgi-present356.exe')],o/'logs/link-client.log')
# Baseline clients retained as evidence only; next batch executes Present356.
for n in ['ge8300-mips.fw','ge8300-d3d-clear.exe','ge8300-d3d-texture.exe','ge8300-d3d-texture-boundary.exe','mt8167-development.cer']:shutil.copyfile(base/'package355'/n,pkg/n)
b=(base/'package355/ge8300-render.inf').read_bytes();encoding='utf-16' if b[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig';s=b.decode(encoding);s=s.replace('0.0.3.55','0.0.3.56');(pkg/'ge8300-render.inf').write_text(s,encoding=encoding)
shutil.copyfile(r/'drivers/gpu/mt8167/display-transport/mt8167-display-transport.inf',transport/'mt8167-display-transport.inf')
sign=Path('C:/mt8167-minimal-service-host/cache/kit81/bin/x86/signtool.exe');thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
for q in [pkg/'ge8300-render.sys',pkg/'ge8300-umd.dll',pkg/'mt8167-dxgi-present356.exe',transport/'mt8167-display-transport.sys']:run([sign,'sign','/fd','SHA256','/sha1',thumb,q]);run([sign,'verify','/pa',q])
for d,cat in [(pkg,'ge8300-render.cat'),(transport,'mt8167-display-transport.cat')]:
 run(['C:/mt8167-hp1033/inputs/signing/bin/Inf2Cat.exe','/driver:'+str(d),'/os:6_3_ARM','/uselocaltime'],o/'logs'/(cat+'.log'));run([sign,'sign','/fd','SHA256','/sha1',thumb,d/cat]);run([sign,'verify','/pa',d/cat])
for q in ['ge8300-render.sys','ge8300-render.inf','ge8300-umd.dll','ge8300-mips.fw']:run([sign,'verify','/pa','/c',pkg/'ge8300-render.cat',pkg/q])
for q in ['mt8167-display-transport.sys','mt8167-display-transport.inf']:run([sign,'verify','/pa','/c',transport/'mt8167-display-transport.cat',transport/q])
manifest=dict(schema='ge8300-unified-present356-v1',package_source_commit=a.source_commit,kernel_binary_source_commit=a.source_commit,native_owner_source_commit=a.source_commit,umd_source_commit=a.source_commit,runtime_build_id='GPU356-unified-present-v1',inf_driverver='11/09/2026,0.0.3.56',compiled_units=records,archives=archives,actual_include_closure=closure,compiler_link_inputs=[dict(file=str(q),sha256=sha(q))for q in ul],baseline_manifest_sha256=sha(base/'package355/signed-manifest.json'),SDK_rebuilt=False,PCO375_rebuilt=False,hardware_verified=False,normal_Windows_display_verified=False,root_reset_fix_proven=False,install_authorized=False,matched_firmware_required=True,display_mode=[800,1280,3200],dma_contract='retained VidMm PFNs; independent DIS0 SG; every page equal; contiguous32; no POST GPU DMA assumption; no CPU render-copy',diagnostic_contract=load(r/'tools/recovery/ge8300-present356-contract.json'))
manifest['unchanged_linked_object_count']=sum(x['sha256']==x['before_sha256']for arc in archives if arc['role']in ['native','kmd']for x in arc['members'])
manifest['outputs']=[dict(name=f.relative_to(pkg).as_posix(),sha256=sha(f))for f in sorted(pkg.rglob('*'))if f.is_file()]
write(o/'build-manifest.json',manifest);shutil.copyfile(o/'build-manifest.json',pkg/'present356-build-manifest.json');manifest['build_manifest_sha256']=sha(pkg/'present356-build-manifest.json');write(pkg/'signed-manifest.json',manifest)
with zipfile.ZipFile(t/'outputs356.zip','w',zipfile.ZIP_DEFLATED)as z:
 for f in o.rglob('*'):
  if f.is_file():z.write(f,f.relative_to(o).as_posix())
print('SIGNED356_CANDIDATE_HARDWARE_UNVERIFIED',sha(pkg/'signed-manifest.json'))
