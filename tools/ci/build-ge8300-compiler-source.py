#!/usr/bin/env python3
"""Build the checked-in PCO/NIR source closure; no previous driver artifacts."""
import argparse,concurrent.futures,hashlib,json,os,shutil,subprocess
from pathlib import Path
from ge8300_compiler_cache import dependency_rows,parse_make_dependencies
p=argparse.ArgumentParser()
for n in ('vc-include','ucrt-include','sdk-shared','sdk-um','vc-arm-libraries','sdk-arm-libraries','ucrt-arm-libraries','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--jobs',type=int,default=2);p.add_argument('--source-root',type=Path);p.add_argument('--resume-cold198',action='store_true');p.add_argument('--attest-original-tool-unchanged',action='store_true');a=p.parse_args();recipe_root=Path(__file__).resolve().parents[2];root=a.source_root.resolve() if a.source_root else recipe_root;out=a.output.resolve()
if not out.is_relative_to(root) or (out.exists() and not a.resume_cold198):raise ValueError('New checkout-local output required; existing cache needs explicit cold198 recovery')
if a.source_root and not a.resume_cold198:raise ValueError('Alternate source root is only for explicit cold198 recovery')
recovered={}
if a.resume_cold198:
 if not a.attest_original_tool_unchanged or not out.is_dir():raise ValueError('Original tool continuity attestation and existing partial cache required')
 spec=json.loads((recipe_root/'tools/ci/ge8300-cold198-partial-pins.json').read_text())
 def checked(path,h):
  if hashlib.sha256(path.read_bytes()).hexdigest()!=h:raise ValueError('Pinned partial/source mismatch: '+str(path))
 if subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()!=spec['source_commit']:raise ValueError('Keep original4b source/path')
 checked(root/'tools/ci/build-ge8300-compiler-source.py',spec['original_recipe_sha256'])
 for name,h in spec['files'].items():
  if name.startswith('compiler/'):checked(out/name[len('compiler/'):],h)
 if (out/'recovery-session.json').exists():raise ValueError('Recovery already started; preserve journals, do not repeat375 build')
 for obj in out.glob('*.obj'):
  if 'compiler/'+obj.name not in spec['files']:raise ValueError('Unpinned extra original object; recover inventory first')
if subprocess.check_output(['git','-C',str(root),'status','--porcelain']):raise ValueError('Clean committed source required')
planpath=root/'third_party/mesa-pco/source-plan.json';plan=json.loads(planpath.read_text())
if a.resume_cold198:checked(planpath,spec['source_plan_sha256'])
def sha(p):return hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest()
for e in plan['files']:
 if sha(root/e['path'])!=e['sha256']:raise ValueError('Source snapshot changed: '+e['path'])
for n in ('vc_include','ucrt_include','sdk_shared','sdk_um','vc_arm_libraries','sdk_arm_libraries','ucrt_arm_libraries'):
 v=getattr(a,n).resolve(strict=True);setattr(a,n,v)
clang=shutil.which('clang');ar=shutil.which('llvm-lib')
if not clang or not ar:raise ValueError('LLVM clang and llvm-lib required')
out.mkdir(exist_ok=a.resume_cold198);(out/'tmp').mkdir(exist_ok=a.resume_cold198);env=dict(os.environ,TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
resource=Path(subprocess.check_output([clang,'-print-resource-dir'],text=True).strip())
vfs=out/'arm-neon-vfs.json';expected_vfs=json.loads(vfs.read_text()) if a.resume_cold198 else None
if not a.resume_cold198:vfs.write_text(json.dumps({'version':0,'case-sensitive':'false','roots':[{'type':'file','name':str(a.vc_include/'arm_neon.h').replace('\\','/'),'external-contents':str(resource/'include/arm_neon.h').replace('\\','/')}]})+'\n')
if a.resume_cold198:
 vr=expected_vfs['roots']
 if len(vr)!=1 or Path(vr[0]['name']).resolve()!=(a.vc_include/'arm_neon.h').resolve() or Path(vr[0]['external-contents']).resolve()!=(resource/'include/arm_neon.h').resolve():raise ValueError('Original VFS/tool resource paths changed')
 (out/'recovery-units').mkdir()
 (out/'recovery-session.json').write_text(json.dumps({'schema':'cold198-reconstructed-recovery-v1','original_source_commit':spec['source_commit'],'source_root':str(root),'recipe_source_commit':subprocess.check_output(['git','-C',str(recipe_root),'rev-parse','HEAD'],text=True).strip(),'historical_exit_codes_complete':False,'historical_compiler_hash_recorded':False,'historical_include_hashes_recorded':False,'original_tool_unchanged_attested':True,'current_clang_sha256':sha(clang),'current_archiver_sha256':sha(ar),'original_partial_pins_sha256':sha(recipe_root/'tools/ci/ge8300-cold198-partial-pins.json')},indent=2)+'\n')
subs={'REPO':str(root),'VC_INCLUDE':str(a.vc_include),'UCRT_INCLUDE':str(a.ucrt_include),'SDK_SHARED':str(a.sdk_shared),'SDK_UM':str(a.sdk_um)}
def expand(s):
 for key,value in subs.items():s=s.replace('${'+key+'}',value)
 if '${' in s:raise ValueError('Unresolved source-plan variable')
 return s
dep_roots={k:Path(v).resolve() for k,v in subs.items()};dep_roots['CLANG_RESOURCE']=resource.resolve()
rows=[]
def compile_one(pair):
 i,u=pair;obj=out/(str(i)+'-'+Path(u['source']).stem+'.obj');cmd=[clang,*map(expand,u['arguments']),'-ivfsoverlay',str(vfs),'-MD','-MF',str(out/(str(i)+'.d')),'-MT','ge8300-unit','-c',str(root/u['source']),'-o',str(obj)]
 if a.resume_cold198 and i<4:
  if not obj.is_file() or obj.read_bytes()[:2]!=b'\xc4\x01':raise ValueError('Recovered object is not ARMNT COFF')
  raw=out/(str(i)+'.d');names=parse_make_dependencies(raw.read_text())
  if str(root/u['source']).replace('\\','/').lower() not in {x.replace('\\','/').lower() for x in names}:raise ValueError('Original .d does not name expected source')
  rc=None;kind='reconstructed-from-fixed4b-recipe-and-original-trace'
 else:
  with (out/(str(i)+'.log')).open('w') as f:rc=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT,env=env).returncode
  kind='actual-recovery-invocation' if a.resume_cold198 else 'actual-cold-invocation'
 row={'component':u['component'],'source':u['source'],'source_sha256':sha(root/u['source']),'command':cmd,'command_recording_kind':kind,'exit_code':rc,'object':obj.name,'object_sha256':sha(obj) if obj.is_file() else None,'dependencies':[],'historical_exit_code_unknown':rc is None}
 if a.resume_cold198:(out/'recovery-units'/f'{i}.json').write_text(json.dumps(row,indent=2)+'\n')
 if rc in (0,None):
  row['dependencies']=dependency_rows(out/(str(i)+'.d'),dep_roots);row['raw_dependency_sha256']=sha(out/(str(i)+'.d'));row['raw_log_sha256']=sha(out/(str(i)+'.log'))
  row['include_hash_capture']='afterfailure-from-original-trace' if rc is None else 'after-real-compile'
 if a.resume_cold198:(out/'recovery-units'/f'{i}.json').write_text(json.dumps(row,indent=2)+'\n')
 return row
with concurrent.futures.ThreadPoolExecutor(max_workers=a.jobs) as pool:rows=list(pool.map(compile_one,enumerate(plan['units'])))
(out/'compile-manifest.json').write_text(json.dumps({'units':rows,'source_plan_sha256':sha(planpath)},indent=2)+'\n')
failed=[u for u in rows if u['exit_code'] not in (0,None)]
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
evidence={'schema':'pco375-reconstructed-afterfailure-v1' if a.resume_cold198 else 'pco375-all-includes-cache-v1','metadata_reconstructed_afterfailure':a.resume_cold198,'historical_exit_codes_complete':not a.resume_cold198,'reused_original_objects':4 if a.resume_cold198 else 0,'new_compile_count':371 if a.resume_cold198 else 375,'include_hashes_recorded_at_original_four_compile':False if a.resume_cold198 else None,'original_tool_hash_independently_recorded':False if a.resume_cold198 else None,'source_plan_sha256':sha(planpath),'compiler_sha256':sha(clang),
 'cache_verifier_sha256':sha(recipe_root/'tools/ci/ge8300_compiler_cache.py'),'build_recipe_sha256':sha(Path(__file__)),'compile_manifest_sha256':sha(out/'compile-manifest.json'),
 'units':[{'source':u['source'],'component':u['component'],'arguments':plan['units'][i]['arguments'],
 'source_sha256':u['source_sha256'],'object_sha256':u['object_sha256'],'dependencies':u['dependencies'],'exit_code':u['exit_code'],'command_recording_kind':u['command_recording_kind'],'include_hash_capture':u.get('include_hash_capture')} for i,u in enumerate(rows)]}
(out/'cache-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
# The UMD linker consumes these verified source-built libraries directly.
(out/'link-manifest.json').write_text(json.dumps({'schema':'compiler-recovered-source-closure-v1' if a.resume_cold198 else 'compiler-source-closure-v1','metadata_reconstructed_afterfailure':a.resume_cold198,'historical_exit_codes_complete':not a.resume_cold198,'strict_cache_qualified':False if a.resume_cold198 else None,'original_objects_reused':4 if a.resume_cold198 else 0,'new_compile_count':371 if a.resume_cold198 else 375,'cache_evidence_sha256':sha(out/'cache-evidence.json'),'source_commit':commit,'source_dirty':False,'source_plan_sha256':sha(planpath),'compiled_units':len(rows),'all_compile_exit_codes_zero':not a.resume_cold198,'all_new_compile_exit_codes_zero':True,'source_built':True,'inputs':inputs,'compiler_version':subprocess.check_output([clang,'--version'],text=True),'compiler_sha256':sha(clang),'runtime_verified':False},indent=2)+'\n')
if a.resume_cold198:
 for name,h in spec['files'].items():
  if name.startswith('compiler/'):checked(out/name[len('compiler/'):],h)
 print('PCO375_RECOVERED4_COMPILED371_COMPLETE_HISTORICAL_EXIT_UNKNOWN')
else:print('PCO_NIR_UTIL_SUPPORT_AND_PAIR_FROM_SOURCE_COMPLETE')
