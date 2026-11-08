#!/usr/bin/env python3
"""332 archives -> real Copy contract integration, transitive preprocessor closure only."""
import argparse,hashlib,json,os,subprocess,sys,shutil,ntpath
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser()
for key in ['baseline-build','old-source','dependency','kernel-kit','vc-include','ucrt-include','link-exe','sign-tool','inf2cat','output']:p.add_argument('--'+key,type=Path,required=True)
p.add_argument('--resume-source');p.add_argument('--cached-source',type=Path)
a=p.parse_args()
out=a.output.resolve();base=a.baseline_build.resolve();package=base/'package332';dep=a.dependency.resolve()
if not out.is_relative_to(root) or (out.exists() and not a.resume_source):raise ValueError('New output or explicit cached-source resume required')
def run(cmd,**kw):return subprocess.run(list(map(str,cmd)),check=True,**kw)
def capture(cmd):return subprocess.check_output(list(map(str,cmd)),text=True).strip()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def load(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def pin(p,h):
 if sha(p)!=h.lower():raise ValueError('Pin mismatch: '+str(p))
commit=capture(['git','-C',root,'rev-parse','HEAD'])
if capture(['git','-C',root,'status','--porcelain']):raise ValueError('Clean committed source required')

pin(package/'signed-manifest.json','55a4d2e6c918275aeccb0364c8af823c1c4f25defd54cf1f3df9753748d753d8')

bm=load(base/'build-manifest.json');pm=load(package/'signed-manifest.json')
for parent,m in [(package,bm),(package,pm)]:
 for r in m['outputs']:pin(parent/r['name'],r['sha256'])

compiler=dep/'compiler';pair=dep/'pair'
pin(compiler/'link-manifest.json','3d5c6feec663ffdc88919f44d2adf31ddb177205623df5e6eb279b87fe47d26b')
pin(pair/'manifest.json','c05e327474cb91c260b341e9c6de91f7074d8f749ea68f35a77ca2ccc1858e8a');pin(pair/'pco_pair.obj',load(pair/'manifest.json')['object_sha256'])
baseline327={'existing_compiler_libraries':load(base/'compiler-library-pins.json')}
for r in baseline327['existing_compiler_libraries']:
 if r['name']!='ge8300-pco-arm32.lib':pin(compiler/r['name'],r['sha256'])
oldroot=a.old_source.resolve();out.mkdir(parents=True,exist_ok=bool(a.resume_source));(out/'tmp').mkdir(exist_ok=True);(out/'generated').mkdir(exist_ok=True)
os.environ.update(TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
pin(base/'copy.usc','5c68917faa051238b23674b64c6b137ef7ff75425dbde9dbc5e1e5a36e02c4a3')
(out/'generated/a04_copy_usc_asset.h').write_text('static const unsigned char a04_copy_usc_asset[152]={'+','.join(hex(b) for b in (base/'copy.usc').read_bytes())+'};\n')
plan=load(root/'third_party/mesa-pco/source-plan.json');recipe=next(r for r in plan['units'] if r['source'].endswith('/pco_sm2.c'))
subs={'REPO':str(root),'VC_INCLUDE':str(a.vc_include),'UCRT_INCLUDE':str(a.ucrt_include),'SDK_SHARED':'C:/mt8167-minimal-service-host/cache/kit81/Include/shared','SDK_UM':'C:/mt8167-minimal-service-host/cache/kit81/Include/um'}
def expand(s):
 for k,v in subs.items():s=s.replace('${'+k+'}',v.replace('\\','/'))
 if '${' in s:raise ValueError('Unexpanded flag')
 return s
cf=[expand(s) for s in recipe['arguments']]+['-DNDEBUG']
nf=next(r['arguments'] for r in bm['compiled_units'] if r['role']=='native' and '/compiler/' not in r['source'])
uf=next(r['arguments'] for r in bm['compiled_units'] if r['role']=='umd');kf=next(r['arguments'] for r in bm['compiled_units'] if r['role']=='kmd')
# Original332 source paths are stable recorded flags, remapped only to this checkout.
nf=[str(s).replace('\\\\','\\').replace('C:\\mt8167-readback-safety332\\source',str(root)) for s in nf]+['-I'+str(out/'generated')]
records=[];archives=[];saved={(r['role'],r['object']):r for r in load(out/'compiled-checkpoint.json')} if a.resume_source else {};oldmap=load(base/'old-source-map.json')['native_unit_sources'];newmap=load(root/'tools/ci/ge8300-native-source-map.json')['native_unit_sources']
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
 if recovered:
  if not a.cached_source or capture(['git','-C',a.cached_source,'rev-parse','HEAD'])!=a.resume_source:raise ValueError('Exact prior source checkout required')
  recovered=pp(source,flags,out/'resume-prior.i',a.cached_source)==pp(source,flags,out/'resume-current.i',root)
 # The prior immutable invocation stopped before its next source read; existing
 # objects were produced by successful checked calls under the same C/H tree.
 if not recovered:run(['clang',*flags,'-c',root/source,'-o',obj])
 records.append({'source':source,'source_sha256':sha(root/source),'object':obj.name,'object_sha256':sha(obj),'arguments':flags,'role':role,'recovered_same_source':recovered,'prior_build_source_commit':a.resume_source if recovered else None})
 (out/'compiled-checkpoint.json').write_text(json.dumps(records,indent=2)+'\n')

def pp(source,flags,path,checkout):
 f=[str(s) if str(s)=='-I'+str(out/'generated') else str(s).replace(str(root),str(checkout)).replace(str(root).replace(chr(92),'/'),str(checkout).replace(chr(92),'/')) for s in flags]
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
   if source.endswith('/mdl_pin.c'):raise ValueError('Changed SEH closure needs MSVC')
   if a.resume_source and (after/n).exists() and sha(after/n)==sha(before/n): (after/n).unlink()
   compile_one(source,after/n,flags,tag)
 for n,source,flags in extra:
  if n in members:raise ValueError('Unexpected preexisting member')
  compile_one(source,after/n,flags,tag);members.append(n)
 archive(tag,leaf,members,before,after)

native_names={unit.replace('\\','-')+'.obj':path for unit,path in oldmap.items()}
extra=[(unit.replace('\\','-')+'.obj',path,nf) for unit,path in newmap.items() if unit not in oldmap]
closure('native','ge8300-native-owner-core.lib',lambda n:native_names[n],lambda s:(cf+['-mfpu=none'] if '/compiler/witness_' in s else nf),extra)
def ksource(n):
 stem=n[:-4];return 'drivers/gpu/mt8167/wddm-render/'+('frontend/'+stem[9:] if stem.startswith('frontend_') and stem!='frontend_memory' else stem)+'.c'
closure('kmd','ge8300-pair-kmd.lib',ksource,lambda s:kf,[])
def usource(n):
 stem=n[:-4]
 if stem.startswith(('umd_','frontend_','compiler_')):stem=stem.replace('_','/',1)
 return 'drivers/gpu/mt8167/wddm-render/'+stem+'.c'
def uflags(s):
 if s.endswith('/compiler/pco_texture2d.c'):return cf
 return [f for f in uf if not(s.endswith('/typed_state.c') and f in ['-D_D3D10_CONSTANTS','-D_D3D10_1_CONSTANTS'])]
closure('umd','ge8300-native-umd.lib',usource,uflags,[])
for name in ['umd_entry.obj','umd_typed_entry.obj']:
 flags=uf+(['-DGEO_LINKED_REAL_FRONTEND=1'] if name=='umd_entry.obj' else ['-DGEO_TYPED_LIMITED_DEVELOPMENT=1','-DGEO_TYPED_NATIVE_PIPELINE10=1'])
 source='drivers/gpu/mt8167/wddm-render/umd/'+('entry.c' if name=='umd_entry.obj' else 'typed_entry.c')
 if pp(source,flags,out/'entry-old.i',oldroot)!=pp(source,flags,out/'entry-new.i',root):compile_one(source,out/'umd'/name,flags,'entry')
 else:shutil.copyfile(base/'umd'/name,out/'umd'/name)
pco_sources={'sm4_adapter.obj':'sm4','pco_sm2_adapter.obj':'pco_sm2','witness_producer.obj':'witness_producer','witness_codec.obj':'witness_codec','witness_info.obj':'witness_info'}
members,before,after=extract(base/'ge8300-pco-arm32.lib','pco')
for name,unit in pco_sources.items():
 source='drivers/gpu/mt8167/wddm-render/compiler/'+unit+'.c'
 if pp(source,cf,out/'pco-old.i',oldroot)!=pp(source,cf,out/'pco-new.i',root):compile_one(source,after/name,cf,'pco')
archive('pco','ge8300-pco-arm32.lib',members,before,after)
kernel=[a.kernel_kit/'Lib/winv6.3/km/arm'/n for n in ['displib.lib','ntoskrnl.lib','hal.lib','libcntpr.lib','bufferoverflowfastfailk.lib']]
run([a.link_exe,'/nologo','/driver','/subsystem:native,6.03','/machine:arm','/entry:DriverEntry','/nodefaultlib','/incremental:no','/base:0x400000',out/'ge8300-pair-kmd.lib',out/'ge8300-native-owner-core.lib',*kernel,'/out:'+str(out/'ge8300-render.sys'),'/map:'+str(out/'ge8300-render.map')])
libs=[out/r['name'] if r['name']=='ge8300-pco-arm32.lib' else compiler/r['name'] for r in baseline327['existing_compiler_libraries']]
run(['lld-link','/dll','/machine:arm','/subsystem:windows,6.03','/entry:_DllMainCRTStartup','/nodefaultlib',out/'umd/umd_entry.obj',out/'umd/umd_typed_entry.obj',out/'ge8300-native-umd.lib',pair/'pco_pair.obj',*libs,'/out:'+str(out/'ge8300-umd.dll'),'/map:'+str(out/'ge8300-umd.map')])
thumb='BCBF6C456C6F0DD0818434763B189488FBF13570'
for name in ['ge8300-render.sys','ge8300-umd.dll']:
 run([a.sign_tool,'sign','/fd','SHA256','/sha1',thumb,out/name]);run([a.sign_tool,'verify','/pa',out/name])
pkg=out/'package336';pkg.mkdir()
for f in package.iterdir():
 if f.is_file() and f.name not in ['ge8300-render.sys','ge8300-umd.dll','ge8300-render.inf','ge8300-render.cat']:
  shutil.copyfile(f,pkg/('baseline332-signed-manifest.json' if f.name=='signed-manifest.json' else f.name))
for n in ['ge8300-render.sys','ge8300-umd.dll']:shutil.copyfile(out/n,pkg/n)
s=(package/'ge8300-render.inf').read_text(encoding='utf-16');old='DriverVer=11/07/2026,0.0.3.32'
if s.count(old)!=1:raise ValueError('Exact332 INF')
(pkg/'ge8300-render.inf').write_text(s.replace(old,'DriverVer=11/07/2026,0.0.3.36'),encoding='utf-16')
run([a.inf2cat,'/driver:'+str(pkg),'/os:6_3_ARM','/uselocaltime']);run([a.sign_tool,'sign','/fd','SHA256','/sha1',thumb,pkg/'ge8300-render.cat'])
for n in ['ge8300-render.inf','ge8300-render.sys','ge8300-umd.dll','ge8300-mips.fw']:run([a.sign_tool,'verify','/pa','/c',pkg/'ge8300-render.cat',pkg/n])
m=dict(pm);m.update(schema='real-tq-copy336-independent-sources-v1',package_source_commit=commit,kernel_binary_source_commit=commit,native_owner_source_commit=commit,umd_source_commit=commit,baseline_manifest_file='baseline332-signed-manifest.json',baseline_manifest_sha256=sha(package/'signed-manifest.json'),compiled_units=records,archives=archives,adapter_contract_version=8,owner_ops_version=3,copy_wire_version=1,copy_transport='actual TQ task + client UFO + SLC/GP + exact MMUCACHE ack + CPU SG return + fresh real IRQ + scheduler fence',client_changed=False,existing_clients_changed=False,inf_driverver='11/07/2026,0.0.3.36',root_reset_fix_proven=False,runtime_verified=False,hardware_verified=False,SG_source_commit='ef0d1dfb',Copy334_source_commit='0bfc91cf9a87c98b0ca1d540d00726159614072d',LockEntire335_source_commit='26b7c8ef2034bb41265dbd4347d952b7a838d7a8')
m['retained_outputs']=[r for r in pm['outputs'] if r['name'] not in ['ge8300-render.sys','ge8300-umd.dll','ge8300-render.inf','ge8300-render.cat']]
for r in m['retained_outputs']:pin(pkg/r['name'],r['sha256'])
m['outputs']=[{'name':r['name'],'sha256':sha(pkg/r['name'])} for r in pm['outputs']]
m['retained_provenance']=[{'name':f.name,'sha256':sha(f)} for f in pkg.iterdir() if f.is_file() and f.name not in [r['name'] for r in m['outputs']]]
m.pop('build_manifest_file',None);m.pop('build_manifest_sha256',None)
(pkg/'copy336-build-manifest.json').write_text(json.dumps(m,indent=2)+'\n');m.update(build_manifest_file='copy336-build-manifest.json',build_manifest_sha256=sha(pkg/'copy336-build-manifest.json'))
(pkg/'signed-manifest.json').write_text(json.dumps(m,indent=2)+'\n');(out/'build-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
print('COPY336_SIGNED_PACKAGE_COMPLETE',commit,len(records))
