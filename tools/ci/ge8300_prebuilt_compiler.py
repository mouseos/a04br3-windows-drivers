#!/usr/bin/env python3
"""Opt-in SHA-fixed historical compiler TOOL input, never a strict cache proof."""
import argparse,hashlib,json,shutil,subprocess
from pathlib import Path

def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def check(path,expected):
 if sha(path)!=expected.lower():raise ValueError('Pinned prebuilt input changed: '+str(path))
def validate(root,cache,api,tools,spec):
 for n,h in spec['manifest_sha256'].items():check(cache/n,h)
 link=read(cache/'link-manifest.json');comp=read(cache/'compile-manifest.json');pair=read(cache/'pair-manifest.json')
 if link['source_commit']!=spec['compiled_source_commit'] or pair['source_commit']!=spec['compiled_source_commit'] or link['source_dirty'] or pair['source_dirty']:raise ValueError('Historical compile identity changed')
 if not link['source_built'] or not link['all_compile_exit_codes_zero'] or link['compiled_units']!=375:raise ValueError('Historical375 compile incomplete')
 planpath=root/'third_party/mesa-pco/source-plan.json';check(planpath,spec['source_plan_sha256']);plan=read(planpath)
 for m in [comp,link,pair]:
  if m['source_plan_sha256']!=spec['source_plan_sha256']:raise ValueError('Historical compiler plan mismatch')
 for f in plan['files']:check(root/f['path'],f['sha256'])
 if len(comp['units'])!=len(plan['units']) or len(plan['units'])!=375:raise ValueError('375TU routing mismatch')
 def norm(s):return s.replace('\\','/')
 live_objects=0;missing_objects=[]
 for row,unit in zip(comp['units'],plan['units']):
  if row['exit_code'] or any(row[k]!=unit[k] for k in ['source','component']):raise ValueError('Historical unit routing failed')
  check(root/unit['source'],row['source_sha256'])
  # The pinned libraries/pair are the real link inputs. Standalone old .obj
  # files are auxiliary provenance, not required to use an immutable archive.
  if (cache/row['object']).is_file():check(cache/row['object'],row['object_sha256']);live_objects+=1
  else:missing_objects.append(row['object'])
  # Compare the RECORDED invocation to source-plan arguments, not old includes
  # to current header bytes. Unknown historical include hashes remain unknown.
  expected=[]
  for a in unit['arguments']:
   for k,v in spec['recorded_include_roots'].items():a=a.replace('${'+k+'}',v)
   expected.append(norm(a))
  cmd=row['command']
  if len(cmd)<7 or cmd[-6]!='-ivfsoverlay' or cmd[-4]!='-c' or cmd[-2]!='-o' or list(map(norm,cmd[1:-6]))!=expected:raise ValueError('Historical arguments differ from current plan')
 for name,h in spec['tool_sha256'].items():check(tools[name],h)
 if link['compiler_sha256']!=spec['tool_sha256']['clang']:raise ValueError('Compiler tool identity mismatch')
 if len(link['inputs'])!=10:raise ValueError('Incomplete compiler/API library set')
 if link['inputs']!=[{k:v for k,v in x.items() if k!='name'} for x in spec['libraries']]:raise ValueError('Historical library manifest changed')
 for lib in spec['libraries']:
  check(cache/lib['name'],lib['sha256'])
  if lib['name'] in api:check(api[lib['name']],lib['sha256'])
 if set(api)!={x['name'] for x in spec['libraries'] if not x['name'].startswith('ge8300-')}:raise ValueError('Exactly six current Microsoft API/CRT library pins required')
 check(cache/'pco_pair.obj',spec['pair_object_sha256'])
 row=next(x for x in comp['units'] if x['component']=='pair')
 if row['object_sha256']!=spec['pair_object_sha256'] or pair['object_sha256']!=row['object_sha256'] or row['source_sha256']!=pair['compiled_source_sha256'] or pair['compiled_source_sha256']!=spec['pair_source_sha256'] or not pair['user_mode_fp'] or pair['kernel_no_fp_applied']:raise ValueError('Historical pair mismatch')
 return {'schema':'pco375-pinned-prebuilt-input-v1','compiler_input_kind':'SHA-fixed historical prebuilt compiler tool','CompilerSourceCommit':spec['compiled_source_commit'],'current_source_commit':subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),'source_plan_sha256':spec['source_plan_sha256'],'manifest_sha256':spec['manifest_sha256'],'includeClosureRecorded':False,'historical_include_hashes':'unknown','strict_cache_qualified':False,'historical_object_hashes_recorded':375,'live_objects_checked':live_objects,'standalone_objects_missing':missing_objects,'live_libraries_checked':10,'live_API_libraries_checked':6,'live_tool_sha256':spec['tool_sha256'],'SDK_rebuilt':0,'PCO_rebuilt':0,'runtime_verified':False}
def main():
 p=argparse.ArgumentParser();p.add_argument('--enable-pinned-prebuilt',action='store_true');p.add_argument('--verify-receipt',type=Path)
 for n in ['cache','vc-arm-libraries','sdk-arm-libraries','ucrt-arm-libraries','stage','output']:p.add_argument('--'+n,type=Path)
 a=p.parse_args();root=Path(__file__).resolve().parents[2];specpath=root/'tools/ci/ge8300-pco375-f4-prebuilt.json';spec=read(specpath)
 if not a.enable_pinned_prebuilt:raise ValueError('Explicit pinned-prebuilt opt-in required')
 if subprocess.check_output(['git','-C',str(root),'status','--porcelain']):raise ValueError('Clean fixed current source required')
 old=None
 if a.verify_receipt:
  old=read(a.verify_receipt)
  if old.get('schema')!='pco375-pinned-prebuilt-input-v1' or old.get('includeClosureRecorded') is not False or old.get('strict_cache_qualified') is not False:raise ValueError('Incorrect prebuilt receipt classification')
  a.cache=Path(old['original_cache_directory']);a.stage=Path(old['staged_directory']);api={k:Path(v) for k,v in old['API_paths'].items()}
 else:
  if not all([a.cache,a.vc_arm_libraries,a.sdk_arm_libraries,a.ucrt_arm_libraries,a.stage,a.output]):raise ValueError('Live cache/API, fresh stage and receipt paths required')
  api={n:d/n for d,ns in [(a.vc_arm_libraries,['libcmt.lib','libvcruntime.lib','libcpmt.lib']),(a.sdk_arm_libraries,['kernel32.lib','synchronization.lib']),(a.ucrt_arm_libraries,['libucrt.lib'])] for n in ns}
 tools={n:shutil.which(n) for n in spec['tool_sha256']}
 if not all(tools.values()):raise ValueError('Existing pinned LLVM tools required')
 result=validate(root,a.cache,api,tools,spec)
 pins={**spec['manifest_sha256'],**{x['name']:x['sha256'] for x in spec['libraries']},'pco_pair.obj':spec['pair_object_sha256']};names=list(pins)
 if not a.stage.resolve().is_relative_to(root):raise ValueError('Staged tools must stay checkout-local')
 if old:
  for k in ['CompilerSourceCommit','current_source_commit','source_plan_sha256','manifest_sha256','includeClosureRecorded','strict_cache_qualified']:
   if old[k]!=result[k]:raise ValueError('Prebuilt receipt binding changed: '+k)
  if old['spec_sha256']!=sha(specpath):raise ValueError('Pinned tool spec changed')
  for n in names:check(a.stage/n,pins[n])
  print(json.dumps(result));return
 if a.stage.exists():raise ValueError('Fresh staged compiler-tool directory required')
 if not a.output.resolve().is_relative_to(root):raise ValueError('Receipt must stay checkout-local')
 a.stage.mkdir(parents=True)
 for n in names:shutil.copyfile(a.cache/n,a.stage/n);check(a.stage/n,pins[n])
 result.update(original_cache_directory=str(a.cache.resolve()),staged_directory=str(a.stage.resolve()),API_paths={k:str(v.resolve()) for k,v in api.items()},spec_sha256=sha(specpath))
 a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
