#!/usr/bin/env python3
"""Opt-in: retain strictly proven compiler core, rebuild changed Windows adapters.
Original cache/raw traces are read-only. No historical include/exit is invented.
"""
import argparse,copy,hashlib,json,os,shutil,subprocess
from pathlib import Path
from ge8300_compiler_cache import verify,dependency_rows
p=argparse.ArgumentParser()
for name in ['baseline-source','cache','expected-link-sha256','vc-include','ucrt-include','sdk-shared','sdk-um','vc-arm-libraries','sdk-arm-libraries','ucrt-arm-libraries','output']:p.add_argument('--'+name,required=True)
a=p.parse_args();root=Path(__file__).resolve().parents[2];base=Path(a.baseline_source).resolve(strict=True);cache=Path(a.cache).resolve(strict=True);out=Path(a.output).resolve()
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(path):return json.loads(Path(path).read_text(encoding='utf-8-sig'))
def write(path,data):Path(path).write_text(json.dumps(data,indent=2)+'\n')
if not out.is_relative_to(root) or out.exists() or out.is_relative_to(cache) or base==root:raise ValueError('Distinct immutable baseline and fresh checkout-local derived output required')
commit=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
if subprocess.check_output(['git','-C',str(root),'status','--porcelain']).strip():raise ValueError('Clean fixed current source required')
link=read(cache/'link-manifest.json');oldplan=read(base/'third_party/mesa-pco/source-plan.json');plan=read(root/'third_party/mesa-pco/source-plan.json')
if subprocess.check_output(['git','-C',str(base),'rev-parse','HEAD'],text=True).strip()!=link['source_commit'] or subprocess.check_output(['git','-C',str(base),'status','--porcelain']).strip():raise ValueError('Baseline source must be the actual clean compiled commit')
if oldplan['units']!=plan['units'] or len(plan['units'])!=375:raise ValueError('TU sources/order/codegen flags changed; not an adapter-only derivation')
for row in plan['files']:
 if sha(root/row['path'])!=row['sha256']:raise ValueError('Current source-plan hash mismatch: '+row['path'])
clang=shutil.which('clang');ar=shutil.which('llvm-lib')
if not clang or not ar:raise ValueError('Existing matching Clang/llvm-lib required')
resource=Path(subprocess.check_output([clang,'-print-resource-dir'],text=True).strip()).resolve()
roots={key:Path(getattr(a,key.lower())).resolve(strict=True) for key in ['VC_INCLUDE','UCRT_INCLUDE','SDK_SHARED','SDK_UM']};roots['CLANG_RESOURCE']=resource
api={name:Path(directory)/name for directory,names in [(a.vc_arm_libraries,['libcmt.lib','libvcruntime.lib','libcpmt.lib']),(a.sdk_arm_libraries,['kernel32.lib','synchronization.lib']),(a.ucrt_arm_libraries,['libucrt.lib'])] for name in names}
out.mkdir();(out/'tmp').mkdir();env=dict(os.environ,TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
vfs=out/'arm-neon-vfs.json';write(vfs,{'version':0,'case-sensitive':'false','roots':[{'type':'file','name':(roots['VC_INCLUDE']/'arm_neon.h').as_posix(),'external-contents':(resource/'include/arm_neon.h').as_posix()}]})
def arguments(unit,source):
 replacements={**roots,'REPO':source};args=[]
 for s in unit['arguments']:
  for key,path in replacements.items():s=s.replace('${'+key+'}',str(path))
  if '${' in s:raise ValueError('Unexpanded source-plan flag')
  args.append(s)
 return args
serial=0
def probe(unit,source):
 global serial
 serial+=1;dep=out/'tmp'/('probe'+str(serial)+'.d')
 with (out/'tmp'/('probe'+str(serial)+'.log')).open('w') as log:
  subprocess.run([clang,*arguments(unit,source),'-ivfsoverlay',str(vfs),'-M','-MF',str(dep),'-MT','ge8300-unit',str(source/unit['source'])],check=True,stdout=log,stderr=subprocess.STDOUT,env=env)
 return dependency_rows(dep,{**roots,'REPO':source})
# This validates all original375 objects, complete original include records,
# original source/recipe/tool identities, APIs and original libraries first.
original_proof=verify(base,cache,a.expected_link_sha256.lower(),{**roots,'REPO':base},clang,api,lambda u:probe(u,base))
oldcomp=read(cache/'compile-manifest.json');oldevidence=read(cache/'cache-evidence.json');rows=[];evidence=[];recompiled=[]
allowed='drivers/gpu/mt8167/wddm-render/compiler/'
for i,(unit,old,inc) in enumerate(zip(plan['units'],oldcomp['units'],oldevidence['units'])):
 current=probe(unit,root);same=current==inc['dependencies'] and sha(root/unit['source'])==inc['source_sha256']
 obj=out/old['object'];row=copy.deepcopy(old);proof=copy.deepcopy(inc)
 if same:
  shutil.copyfile(cache/old['object'],obj)
  for suffix in ['.d','.log']:
   path=cache/(str(i)+suffix)
   if not path.is_file():raise ValueError('Original raw dependency/log missing: '+str(path))
   shutil.copyfile(path,out/path.name)
  row.update(compile_source_commit=link['source_commit'],reuse_kind='strict-original-object-and-unmodified-trace',original_compile_row_sha256=hashlib.sha256(json.dumps(old,sort_keys=True,separators=(',',':')).encode()).hexdigest())
  proof.update(compile_source_commit=link['source_commit'],include_hash_capture='retained-original-compile',original_evidence_row_sha256=hashlib.sha256(json.dumps(inc,sort_keys=True,separators=(',',':')).encode()).hexdigest())
 else:
  if not unit['source'].startswith(allowed):raise ValueError('A compiler-core TU changed; refuse rebuilding unrelated PCO/NIR/util: '+unit['source'])
  cmd=[clang,*arguments(unit,root),'-ivfsoverlay',str(vfs),'-MD','-MF',str(out/(str(i)+'.d')),'-MT','ge8300-unit','-c',str(root/unit['source']),'-o',str(obj)]
  with (out/(str(i)+'.log')).open('w') as log:rc=subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT).returncode
  if rc:raise ValueError('Adapter compile failed; preserve output/log: '+unit['source'])
  deps=dependency_rows(out/(str(i)+'.d'),{**roots,'REPO':root})
  if deps!=current:raise ValueError('Dependencies changed between probe and actual compile')
  row={'component':unit['component'],'source':unit['source'],'source_sha256':sha(root/unit['source']),'command':cmd,'command_recording_kind':'actual-adapter-invocation','exit_code':rc,'object':obj.name,'object_sha256':sha(obj),'dependencies':deps,'raw_dependency_sha256':sha(out/(str(i)+'.d')),'raw_log_sha256':sha(out/(str(i)+'.log')),'historical_exit_code_unknown':False,'include_hash_capture':'after-real-compile','compile_source_commit':commit}
  proof={key:row[key] for key in ['source','component','source_sha256','object_sha256','dependencies','exit_code','command_recording_kind','include_hash_capture','compile_source_commit']};proof['arguments']=unit['arguments'];recompiled.append(unit['source'])
 if sha(obj)!=row['object_sha256']:raise ValueError('Derived object differs from its real record')
 rows.append(row);evidence.append(proof)
inputs=[]
for component in ['pco','nir','util','support']:
 name='ge8300-'+component+'-arm32.lib';dst=out/name
 if any(u['source'] in recompiled and u['component']==component for u in rows):
  subprocess.run([ar,'/machine:arm','/out:'+str(dst),*[str(out/u['object']) for u in rows if u['component']==component]],check=True,env=env)
 else:shutil.copyfile(cache/name,dst)
 inputs.append({'path':str(dst),'sha256':sha(dst),'role':'derived source compiler '+component})
for name,path in api.items():shutil.copyfile(path,out/name);inputs.append({'path':str(out/name),'sha256':sha(out/name),'role':'unchanged Microsoft ARM API/tool input'})
parent=out/'parent-evidence';parent.mkdir()
for name in ['compile-manifest.json','link-manifest.json','cache-evidence.json','pair-manifest.json']:shutil.copyfile(cache/name,parent/name)
write(parent/'strict-verification.json',original_proof)
planhash=sha(root/'third_party/mesa-pco/source-plan.json');write(out/'compile-manifest.json',{'units':rows,'source_plan_sha256':planhash})
pair=next(row for row in rows if row['component']=='pair');shutil.copyfile(out/pair['object'],out/'pco_pair.obj')
write(out/'pair-manifest.json',{'source_commit':commit,'source_dirty':False,'user_mode_fp':True,'kernel_no_fp_applied':False,'dependencies':{},'object_sha256':sha(out/'pco_pair.obj'),'source_plan_sha256':planhash,'compiled_source_sha256':pair['source_sha256'],'source_built':True,'compile_source_commit':pair['compile_source_commit']})
write(out/'cache-evidence.json',{'schema':'pco375-adapter-derived-cache-v1','source_plan_sha256':planhash,'compiler_sha256':sha(clang),'archiver_sha256':sha(ar),'cache_verifier_sha256':sha(root/'tools/ci/ge8300_compiler_cache.py'),'build_recipe_sha256':sha(Path(__file__)),'compile_manifest_sha256':sha(out/'compile-manifest.json'),'units':evidence,'parent_evidence_sha256':{p.name:sha(p) for p in parent.iterdir()},'parent_compile_source_commit':link['source_commit'],'adapter_recompiled_sources':recompiled,'compiler_core_recompiled':0,'historical_exit_codes_complete':True})
write(out/'link-manifest.json',{'schema':'compiler-source-closure-v1','source_commit':commit,'source_dirty':False,'source_plan_sha256':planhash,'cache_evidence_sha256':sha(out/'cache-evidence.json'),'compiled_units':375,'all_compile_exit_codes_zero':True,'source_built':True,'inputs':inputs,'compiler_version':subprocess.check_output([clang,'--version'],text=True),'compiler_sha256':sha(clang),'runtime_verified':False,'derivation_recipe_sha256':sha(Path(__file__)),'archiver_sha256':sha(ar),'derivation':'strict-parent-plus-actual-adapter-compiles','compiler_core_recompiled':0,'adapter_recompiled_sources':recompiled})
result=verify(root,out,sha(out/'link-manifest.json'),{**roots,'REPO':root},clang,api,lambda u:probe(u,root),allow_adapter_derived=True)
write(out/'derived-verification.json',result);print(json.dumps({'derived_cache':str(out),'link_manifest_sha256':sha(out/'link-manifest.json'),'adapter_compiles':len(recompiled),'compiler_core_compiles':0,'SDK_rebuilt':0,'hardware_verified':False}))
