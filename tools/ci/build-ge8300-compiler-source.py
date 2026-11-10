#!/usr/bin/env python3
"""Build the checked-in PCO/NIR source closure; no previous driver artifacts."""
import argparse,concurrent.futures,hashlib,json,os,shutil,subprocess
from pathlib import Path
from ge8300_compiler_cache import dependency_rows
p=argparse.ArgumentParser()
for n in ('vc-include','ucrt-include','sdk-shared','sdk-um','vc-arm-libraries','sdk-arm-libraries','ucrt-arm-libraries','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--jobs',type=int,default=2);a=p.parse_args();root=Path(__file__).resolve().parents[2];out=a.output.resolve()
if not out.is_relative_to(root) or out.exists():raise ValueError('New checkout-local output required')
if subprocess.check_output(['git','-C',str(root),'status','--porcelain']):raise ValueError('Clean committed source required')
planpath=root/'third_party/mesa-pco/source-plan.json';plan=json.loads(planpath.read_text())
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
for e in plan['files']:
 if sha(root/e['path'])!=e['sha256']:raise ValueError('Source snapshot changed: '+e['path'])
for n in ('vc_include','ucrt_include','sdk_shared','sdk_um','vc_arm_libraries','sdk_arm_libraries','ucrt_arm_libraries'):
 v=getattr(a,n).resolve(strict=True);setattr(a,n,v)
clang=shutil.which('clang');ar=shutil.which('llvm-lib')
if not clang or not ar:raise ValueError('LLVM clang and llvm-lib required')
out.mkdir();(out/'tmp').mkdir();env=dict(os.environ,TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
resource=Path(subprocess.check_output([clang,'-print-resource-dir'],text=True).strip())
vfs=out/'arm-neon-vfs.json';vfs.write_text(json.dumps({'version':0,'case-sensitive':'false','roots':[{'type':'file','name':str(a.vc_include/'arm_neon.h').replace('\\','/'),'external-contents':str(resource/'include/arm_neon.h').replace('\\','/')}]})+'\n')
subs={'REPO':str(root),'VC_INCLUDE':str(a.vc_include),'UCRT_INCLUDE':str(a.ucrt_include),'SDK_SHARED':str(a.sdk_shared),'SDK_UM':str(a.sdk_um)}
def expand(s):
 for key,value in subs.items():s=s.replace('${'+key+'}',value)
 if '${' in s:raise ValueError('Unresolved source-plan variable')
 return s
dep_roots={k:Path(v).resolve() for k,v in subs.items()};dep_roots['CLANG_RESOURCE']=resource.resolve()
rows=[]
def compile_one(pair):
 i,u=pair;obj=out/(str(i)+'-'+Path(u['source']).stem+'.obj');cmd=[clang,*map(expand,u['arguments']),'-ivfsoverlay',str(vfs),'-MD','-MF',str(out/(str(i)+'.d')),'-MT','ge8300-unit','-c',str(root/u['source']),'-o',str(obj)]
 with (out/(str(i)+'.log')).open('w') as f:rc=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,env=env).returncode
 return {'component':u['component'],'source':u['source'],'source_sha256':sha(root/u['source']),'command':cmd,'exit_code':rc,'object':obj.name,'object_sha256':sha(obj) if not rc else None,'dependencies':dependency_rows(out/(str(i)+'.d'),dep_roots) if not rc else []}
with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:rows=list(pool.map(compile_one,enumerate(plan['units'])))
(out/'compile-manifest.json').write_text(json.dumps({'units':rows,'source_plan_sha256':sha(planpath)},indent=2)+'\n')
failed=[u for u in rows if u['exit_code']]
if failed:raise SystemExit('PCO source compilation failed: '+str([(u['source'],u['exit_code']) for u in failed[:8]]))
inputs=[]
for component in ('pco','nir','util','support'):
 lib=out/('ge8300-'+component+'-arm32.lib');subprocess.run([ar,'/machine:arm','/out:'+str(lib),*[str(out/u['object']) for u in rows if u['component']==component]],check=True,env=env)
 inputs.append({'path':str(lib),'sha256':sha(lib),'role':'source-built compiler '+component})
# Toolchain runtime/import libraries are prerequisites, never old GPU objects.
for directory,names in [(a.vc_arm_libraries,['libcmt.lib','libvcruntime.lib','libcpmt.lib']),(a.ucrt_arm_libraries,['libucrt.lib']),(a.sdk_arm_libraries,['kernel32.lib','synchronization.lib'])]:
 for n in names:
  src=directory/n;dst=out/n;shutil.copyfile(src,dst);inputs.append({'path':str(dst),'sha256':sha(dst),'role':'Microsoft ARM toolchain CRT or SDK'})
commit=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
pair=next(u for u in rows if u['component']=='pair');shutil.copyfile(out/pair['object'],out/'pco_pair.obj')
(out/'pair-manifest.json').write_text(json.dumps({'source_commit':commit,'source_dirty':False,'user_mode_fp':True,'kernel_no_fp_applied':False,'dependencies':{},'object_sha256':sha(out/'pco_pair.obj'),'source_plan_sha256':sha(planpath),'compiled_source_sha256':pair['source_sha256'],'source_built':True},indent=2)+'\n')
# Evidence is generated at compilation, including system includes (-MD, not
# -MMD). An old manifest without this evidence is never retroactively blessed.
evidence={'schema':'pco375-all-includes-cache-v1','source_plan_sha256':sha(planpath),'compiler_sha256':sha(clang),
 'cache_verifier_sha256':sha(root/'tools/ci/ge8300_compiler_cache.py'),'build_recipe_sha256':sha(Path(__file__)),'compile_manifest_sha256':sha(out/'compile-manifest.json'),
 'units':[{'source':u['source'],'component':u['component'],'arguments':plan['units'][i]['arguments'],
 'source_sha256':u['source_sha256'],'object_sha256':u['object_sha256'],'dependencies':u['dependencies']} for i,u in enumerate(rows)]}
(out/'cache-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
# The UMD linker consumes these verified source-built libraries directly.
(out/'link-manifest.json').write_text(json.dumps({'schema':'compiler-source-closure-v1','cache_evidence_sha256':sha(out/'cache-evidence.json'),'source_commit':commit,'source_dirty':False,'source_plan_sha256':sha(planpath),'compiled_units':len(rows),'all_compile_exit_codes_zero':True,'source_built':True,'inputs':inputs,'compiler_version':subprocess.check_output([clang,'--version'],text=True),'compiler_sha256':sha(clang),'runtime_verified':False},indent=2)+'\n')
print('PCO_NIR_UTIL_SUPPORT_AND_PAIR_FROM_SOURCE_COMPLETE')
