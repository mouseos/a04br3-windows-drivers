#!/usr/bin/env python3
"""Fixed327 -> ABI4 candidate; compile only changed transitive source closure."""
import argparse,hashlib,json,os,subprocess,sys,shutil,ntpath
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
for key in ['baseline-build','baseline-package','dependency','kernel-kit','vc-include','ucrt-include','link-exe','sign-tool','inf2cat','output']:p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--resume-source')
a=p.parse_args();out=a.output.resolve();base=a.baseline_build.resolve();package=a.baseline_package.resolve();dep=a.dependency.resolve()
if not out.is_relative_to(root) or (out.exists() and not a.resume_source):raise ValueError('Fresh checkout-local output or explicit source-pinned resume required')
def run(cmd,**kw):return subprocess.run(list(map(str,cmd)),check=True,**kw)
def capture(cmd):return subprocess.check_output(list(map(str,cmd)),text=True).strip()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def pin(p,h):
 if sha(p)!=h.lower():raise ValueError('Pin mismatch: '+str(p))
commit=capture(['git','-C',root,'rev-parse','HEAD'])
if capture(['git','-C',root,'status','--porcelain']):raise ValueError('Clean committed source required')
run([sys.executable,'-B',root/'tools/ci/verify-ge8300-fullsource.py'])
pin(package/'signed-manifest.json','7e4ccf146cb8081a93e85b7f83ac24a50ea6f66f8888f4403ccaf08f19433663')
pin(base/'build-manifest.json','1c74110363c68d2270513d3754f06f554632b36d184fa3803290f8cd07fc47c6')
bm=load(base/'build-manifest.json');pm=load(package/'signed-manifest.json')
for parent,m in [(base,bm),(package,pm)]:
 for r in m['outputs']:pin(parent/r['name'],r['sha256'])
compiler=dep/'compiler';pair=dep/'pair';pin(compiler/'link-manifest.json','3d5c6feec663ffdc88919f44d2adf31ddb177205623df5e6eb279b87fe47d26b');pin(pair/'manifest.json','c05e327474cb91c260b341e9c6de91f7074d8f749ea68f35a77ca2ccc1858e8a');pin(pair/'pco_pair.obj',load(pair/'manifest.json')['object_sha256'])
for r in bm['existing_compiler_libraries']:pin(compiler/r['name'],r['sha256'])
if a.resume_source:
 if not out.exists() or len(a.resume_source)!=40:raise ValueError('Existing output and full previous source pin required')
 changed=capture(['git','-C',root,'diff','--name-only',a.resume_source,commit]).splitlines()
 if any(n not in ['tools/ci/build-ge8300-native-shader329.py','README.md','drivers/gpu/mt8167/wddm-render/compiler/witness_codec.c','tests/ge8300_witness_codegen.c','third_party/mesa-pco/source-plan.json','tools/ci/ge8300-fullsource-origins.json'] for n in changed):raise ValueError('Resume source changed outside the reviewed helper')
out.mkdir(parents=True,exist_ok=bool(a.resume_source));(out/'tmp').mkdir(exist_ok=bool(a.resume_source));os.environ.update(TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
# baseline is source/build/filter327
oldroot=base.parent.parent
plan=load(root/'third_party/mesa-pco/source-plan.json');recipe=next(r for r in plan['units'] if r['source'].endswith('/pco_sm2.c'))
subs={'REPO':str(root),'VC_INCLUDE':str(a.vc_include),'UCRT_INCLUDE':str(a.ucrt_include),'SDK_SHARED':str(a.kernel_kit/'Include/shared'),'SDK_UM':str(a.kernel_kit/'Include/um')}
# UM headers are the already-migrated minimal8.1 tree, not a new SDK setup.
kit81=Path('C:/mt8167-minimal-service-host/cache/kit81/Include');subs['SDK_SHARED']=str(kit81/'shared');subs['SDK_UM']=str(kit81/'um')
def expand(s):
 for k,v in subs.items():s=s.replace('${'+k+'}',v.replace('\\','/'))
 if '${' in s:raise ValueError('Unexpanded plan')
 return s
cf=[expand(s) for s in recipe['arguments']]+['-DNDEBUG']
nf=next(r['arguments'] for r in bm['compiled_units'] if r['role']=='native')
uf=next(r['arguments'] for r in bm['compiled_units'] if r['role']=='UMD')
kf=next(r['arguments'] for r in bm['compiled_units'] if r['role']=='KMD')
nf=[str(s).replace(str(oldroot),str(root)) for s in nf]
saved={ (r['role'],r['object']):r for r in load(out/'compiled-checkpoint.json')} if a.resume_source and (out/'compiled-checkpoint.json').exists() else {}
records=[];archives=[];newmap=load(root/'tools/ci/ge8300-native-source-map.json')['native_unit_sources']
oldmap=load(oldroot/'tools/ci/ge8300-native-source-map.json')['native_unit_sources']
# Compiler adapters are changed; 31 existing PCO core archive members survive.
def extract(path,tag):
 before=out/(tag+'-before');after=out/tag;before.mkdir(exist_ok=bool(a.resume_source));after.mkdir(exist_ok=bool(a.resume_source))
 members=[ntpath.basename(n) for n in capture(['llvm-ar','t',path]).splitlines()]
 if len(members)!=len(set(members)):raise ValueError('Duplicate archive member')
 run(['llvm-ar','x','--output='+str(before),path])
 for m in members:
  if not (a.resume_source and (after/m).exists()):shutil.copyfile(before/m,after/m)
 return members,before,after

def compile_one(source,obj,flags,role):
 prior=saved.get((role,obj.name))
 recovered=bool(a.resume_source and obj.exists() and prior and prior['source_sha256']==sha(root/source) and prior['arguments']==flags and prior['object_sha256']==sha(obj))
 # The prior immutable invocation stopped before its next source read; existing
 # objects were produced by successful checked calls under the same C/H tree.
 if not recovered:run(['clang',*flags,'-c',root/source,'-o',obj])
 records.append({'source':source,'source_sha256':sha(root/source),'object':obj.name,'object_sha256':sha(obj),'arguments':flags,'role':role,'recovered_same_source':recovered,'prior_build_source_commit':a.resume_source if recovered else None})
 (out/'compiled-checkpoint.json').write_text(json.dumps(records,indent=2)+'\n')

def pp(source,flags,path,checkout):
 f=[str(s).replace(str(root),str(checkout)) for s in flags]
 if source.endswith('/mdl_pin.c'):f+=['-U__clang__']
 run(['clang',*f,'-E','-P',checkout/source,'-o',path],stdout=subprocess.DEVNULL)
 return path.read_bytes().replace(str(checkout).encode(),b'${REPO}').replace(str(checkout).replace('\\','/').encode(),b'${REPO}')

def archive(tag,leaf,members,before,after):
 paths=[str(after/m) for m in members]
 run(['llvm-lib','/machine:arm','/out:'+str(out/leaf),*reversed(paths)])
 actual=[ntpath.basename(n) for n in capture(['llvm-ar','t',out/leaf]).splitlines()]
 if actual!=members:raise ValueError('Archive member/order mismatch')
 rows=[]
 for n in members:rows.append({'name':n,'sha256':sha(after/n),'before_sha256':sha(before/n) if (before/n).exists() else None,'compiled':any(r['object']==n and r['role']==tag for r in records)})
 archives.append({'role':tag,'file':leaf,'sha256':sha(out/leaf),'members':rows})

def closure(tag,leaf,source_for,flags_for,extra):
 members,before,after=extract(base/leaf,tag)
 for n in members:
  source=source_for(n);flags=flags_for(source)
  old=pp(source,flags,out/(tag+'-old.i'),oldroot);new=pp(source,flags,out/(tag+'-new.i'),root)
  if old!=new:
   if source.endswith('/mdl_pin.c'):raise ValueError('Changed SEH closure requires explicit MSVC build')
   if a.resume_source and (after/n).exists() and sha(after/n)==sha(before/n): (after/n).unlink()
   compile_one(source,after/n,flags,tag)
 for n,source,flags in extra:
  if n in members:raise ValueError('Unexpected preexisting member')
  compile_one(source,after/n,flags,tag);members.append(n)
 archive(tag,leaf,members,before,after)

native_names={unit.replace('\\','-')+'.obj':path for unit,path in oldmap.items()}
closure('native','ge8300-native-owner-core.lib',lambda n:native_names[n],lambda s:nf,[('wddm-render-frontend-shader_draw_protocol.obj','drivers/gpu/mt8167/wddm-render/frontend/shader_draw_protocol.c',nf),('wddm-render-compiler-witness_codec.obj','drivers/gpu/mt8167/wddm-render/compiler/witness_codec.c',cf+['-mfpu=none']),('wddm-render-compiler-witness_info.obj','drivers/gpu/mt8167/wddm-render/compiler/witness_info.c',cf+['-mfpu=none'])])
def ksource(n):
 stem=n[:-4];return 'drivers/gpu/mt8167/wddm-render/'+('frontend/'+stem[9:] if stem.startswith('frontend_') and stem!='frontend_memory' else stem)+'.c'
closure('kmd','ge8300-pair-kmd.lib',ksource,lambda s:kf,[])
def usource(n):
 stem=n[:-4]
 if stem.startswith(('umd_','frontend_','compiler_')):stem=stem.replace('_','/',1)
 return 'drivers/gpu/mt8167/wddm-render/'+stem+'.c'
def uflags(s):
 if s.endswith('/compiler/pco_texture2d.c'):return cf
 return [f for f in uf if not (s.endswith('/typed_state.c') and f in ['-D_D3D10_CONSTANTS','-D_D3D10_1_CONSTANTS'])]
closure('umd','ge8300-native-umd.lib',usource,uflags,[('frontend_shader_draw_protocol.obj','drivers/gpu/mt8167/wddm-render/frontend/shader_draw_protocol.c',uf)])
for n in ['umd_entry.obj','umd_typed_entry.obj']:
 flags=uf+(['-DGEO_LINKED_REAL_FRONTEND=1'] if n=='umd_entry.obj' else ['-DGEO_TYPED_LIMITED_DEVELOPMENT=1','-DGEO_TYPED_NATIVE_PIPELINE10=1'])
 source='drivers/gpu/mt8167/wddm-render/umd/'+('entry.c' if n=='umd_entry.obj' else 'typed_entry.c')
 if pp(source,flags,out/'entry-old.i',oldroot)!=pp(source,flags,out/'entry-new.i',root):compile_one(source,out/'umd'/n,flags,'entry')
 else:shutil.copyfile(base/'umd'/n,out/'umd'/n)
members,before,after=extract(compiler/'ge8300-pco-arm32.lib','pco')
for name,unit in [('sm4_adapter.obj','sm4'),('pco_sm2_adapter.obj','pco_sm2'),('witness_producer.obj','witness_producer'),('witness_codec.obj','witness_codec'),('witness_info.obj','witness_info')]:
 if a.resume_source and (after/name).exists() and (before/name).exists() and sha(after/name)==sha(before/name):(after/name).unlink()
 compile_one('drivers/gpu/mt8167/wddm-render/compiler/'+unit+'.c',after/name,cf,'pco')
 if name not in members:members.append(name)
archive('pco','ge8300-pco-arm32.lib',members,before,after)
kernel=[a.kernel_kit/'Lib/winv6.3/km/arm'/n for n in ['displib.lib','ntoskrnl.lib','hal.lib','libcntpr.lib','bufferoverflowfastfailk.lib']]
run([a.link_exe,'/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/base:0x400000',out/'ge8300-pair-kmd.lib',out/'ge8300-native-owner-core.lib',*kernel,'/out:'+str(out/'ge8300-render.sys'),'/map:'+str(out/'ge8300-render.map')])
libs=[out/r['name'] if r['name']=='ge8300-pco-arm32.lib' else compiler/r['name'] for r in bm['existing_compiler_libraries']]
run(['lld-link','/dll','/machine:arm','/subsystem:windows,6.03','/entry:_DllMainCRTStartup','/nodefaultlib',out/'umd/umd_entry.obj',out/'umd/umd_typed_entry.obj',out/'ge8300-native-umd.lib',pair/'pco_pair.obj',*libs,'/out:'+str(out/'ge8300-umd.dll'),'/map:'+str(out/'ge8300-umd.map')])
thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
for name in ['ge8300-render.sys','ge8300-umd.dll']:
 run([a.sign_tool,'sign','/fd','SHA256','/sha1',thumb,out/name]);run([a.sign_tool,'verify','/pa',out/name])
# Preserve baseline source/closure graph; only package INF/CAT/SYS/DLL change.
pkg=out/'package329';pkg.mkdir()
for f in package.iterdir():
 if not f.is_file() or f.name in ['ge8300-render.sys','ge8300-umd.dll','ge8300-render.inf','ge8300-render.cat']:continue
 shutil.copyfile(f,pkg/('baseline327-signed-manifest.json' if f.name=='signed-manifest.json' else f.name))
for n in ['ge8300-render.sys','ge8300-umd.dll']:shutil.copyfile(out/n,pkg/n)
inf=(package/'ge8300-render.inf').read_text(encoding='utf-16');old='DriverVer=11/07/2026,0.0.3.27'
if inf.count(old)!=1:raise ValueError('Exact327 INF required')
(pkg/'ge8300-render.inf').write_text(inf.replace(old,'DriverVer=11/07/2026,0.0.3.29'),encoding='utf-16')
run([a.inf2cat,'/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime']);run([a.sign_tool,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.cat'])
for n in ['ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw']:run([a.sign_tool,'verify','/pa','/c',pkg/'ge8300-render.cat',pkg/n])
for r in pm['outputs']:pin(package/r['name'],r['sha256'])
manifest={'schema':'native-alu329-independent-sources-v1','package_source_commit':commit,'kernel_binary_source_commit':commit,'native_owner_source_commit':commit,'umd_source_commit':commit,'baseline_manifest_file':'baseline327-signed-manifest.json','baseline_manifest_sha256':sha(package/'signed-manifest.json'),'compiled_units':records,'archives':archives,'adapter_contract_version':5,'shader_draw_version':4,'texture_draw_version':3,'sampler_profile_version':4,'MDL_resolve_version':1,'control_version':2,'PCO_core_rebuilt':False,'SDK_rebuilt':False,'firmware_changed':False,'client_changed':False,'hardware_verified':False,'retained_outputs':[r for r in pm['outputs'] if r['name'] not in ['ge8300-render.sys','ge8300-umd.dll','ge8300-render.inf','ge8300-render.cat']],'outputs':[{'name':n,'sha256':sha(pkg/n)} for n in ['ge8300-render.inf','ge8300-render.cat','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw','ge8300-d3d-clear.exe','ge8300-d3d-texture.exe','mt8167-development.cer']]}
manifest.update({'inf_driverver':'11/07/2026,0.0.3.29','native_resolve_ABI_version':1,'driver_wire_version':4,'shader_witness_ISA':0x32901,'standard_profile_qualified':False,'runtime_verified':False,'baseline_client_source_commit':pm.get('client_source_commit'),'new_client_source_commit':pm.get('new_client_source_commit'),'new_client_file':'ge8300-d3d-texture.exe','kernel_changed':True,'native_owner_changed':True,'umd_changed':True,'retained_provenance':[{'name':f.name,'sha256':sha(f)} for f in pkg.iterdir() if f.is_file() and f.name not in [r['name'] for r in manifest['outputs']]]})
(pkg/'signed-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
(out/'build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('NATIVE_ALU329_SIGNED_PACKAGE_COMPLETE',commit,len(records))
