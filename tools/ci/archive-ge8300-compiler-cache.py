#!/usr/bin/env python3
"""PCO375実体とcompile時include証拠を回収する。compile/SDK再buildはしない。"""
import argparse,hashlib,json,os,re,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
LIBRARIES={f'ge8300-{c}-arm32.lib' for c in ['pco','nir','util','support']}|{'libcmt.lib','libvcruntime.lib','libcpmt.lib','libucrt.lib','kernel32.lib','synchronization.lib'}
def digest(data):return hashlib.sha256(data).hexdigest()
def sha(p):
 with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def basename(p):return str(p).replace('\\','/').rsplit('/',1)[-1]
def pin(p,h):
 if sha(p)!=h.lower():raise ValueError('Cache/source SHA mismatch: '+str(p))
def validate(root,cache,expected):
 pin(cache/'link-manifest.json',expected)
 if not isinstance(read(cache/'arm-neon-vfs.json'),dict):raise ValueError('Actual VFS overlay record missing')
 link=read(cache/'link-manifest.json');comp=read(cache/'compile-manifest.json');pair=read(cache/'pair-manifest.json');evidence=read(cache/'cache-evidence.json');plan=read(root/'third_party/mesa-pco/source-plan.json')
 if link.get('schema')!='compiler-source-closure-v1' or link.get('source_dirty') or not link.get('source_built') or not link.get('all_compile_exit_codes_zero') or link.get('compiled_units')!=375:raise ValueError('Successful clean real375 compiler manifest required')
 if evidence.get('schema')!='pco375-all-includes-cache-v1':raise ValueError('Compile-time all-include evidence required; no reconstruction')
 pin(cache/'cache-evidence.json',link['cache_evidence_sha256']);pin(cache/'compile-manifest.json',evidence['compile_manifest_sha256'])
 pin(root/'tools/ci/build-ge8300-compiler-source.py',evidence['build_recipe_sha256']);pin(root/'tools/ci/ge8300_compiler_cache.py',evidence['cache_verifier_sha256'])
 planhash=sha(root/'third_party/mesa-pco/source-plan.json')
 if any(x.get('source_plan_sha256')!=planhash for x in [link,comp,pair,evidence]):raise ValueError('Source plan identity mismatch')
 if evidence['compiler_sha256']!=link['compiler_sha256']:raise ValueError('Compiler identity mismatch')
 for row in plan['files']:pin(root/row['path'],row['sha256'])
 if len(plan['units'])!=375 or len(comp['units'])!=375 or len(evidence['units'])!=375:raise ValueError('Missing375 compile/include records')
 objects=set();dependencies={}
 for i,(unit,row,proof) in enumerate(zip(plan['units'],comp['units'],evidence['units'])):
  if row.get('exit_code')!=0 or any(unit[k]!=row[k] or unit[k]!=proof[k] for k in ['source','component']):raise ValueError('Compile route/exit mismatch')
  if proof['arguments']!=unit['arguments'] or proof['dependencies']!=row['dependencies'] or proof['source_sha256']!=row['source_sha256'] or proof['object_sha256']!=row['object_sha256']:raise ValueError('Compile/include proof inconsistent')
  pin(root/unit['source'],row['source_sha256'])
  obj=row['object']
  if basename(obj)!=obj or obj in objects:raise ValueError('Object identity escaped/duplicated')
  objects.add(obj);pin(cache/obj,row['object_sha256'])
  cmd=row['command']
  if '-MD' not in cmd or '-MMD' in cmd or cmd[cmd.index('-MT')+1]!='ge8300-unit' or basename(cmd[cmd.index('-MF')+1])!=f'{i}.d':raise ValueError('Missing actual system-include dependency invocation')
  if not (cache/f'{i}.d').is_file() or not (cache/f'{i}.log').is_file():raise ValueError('Raw dependency/log missing')
  deps=proof['dependencies']
  if not deps or '${REPO}/'+unit['source'] not in {d['path'] for d in deps}:raise ValueError('Incomplete include source trace')
  for dep in deps:
   path,h=dep['path'],dep['sha256']
   if not re.fullmatch('[0-9a-f]{64}',h) or not path.startswith('${'):raise ValueError('Malformed recorded dependency')
   if path in dependencies and dependencies[path]!=h:raise ValueError('Include changed during375 compile')
   dependencies[path]=h
   if path.startswith('${REPO}/'):
    p=(root/path[len('${REPO}/'):]).resolve(strict=True)
    if not p.is_relative_to(root):raise ValueError('Include escaped source')
    pin(p,h)
 if len(link['inputs'])!=10 or {basename(x['path']) for x in link['inputs']}!=LIBRARIES:raise ValueError('All ten real compiler/API libraries required')
 for row in link['inputs']:pin(cache/basename(row['path']),row['sha256'])
 pairs=[r for r in comp['units'] if r['component']=='pair']
 if len(pairs)!=1 or pair.get('source_commit')!=link['source_commit'] or pair.get('source_dirty') or not pair.get('source_built') or not pair.get('user_mode_fp') or pair.get('kernel_no_fp_applied'):raise ValueError('Pair source provenance mismatch')
 if pair['object_sha256']!=pairs[0]['object_sha256'] or pair['compiled_source_sha256']!=pairs[0]['source_sha256']:raise ValueError('Pair compile identity mismatch')
 pin(cache/'pco_pair.obj',pair['object_sha256'])
 return {'compiled_source_commit':link['source_commit'],'link_manifest_sha256':sha(cache/'link-manifest.json'),'cache_evidence_sha256':sha(cache/'cache-evidence.json'),'source_plan_sha256':planhash,'compiled_units':375,'unique_objects':len(objects),'libraries':10,'raw_dependency_files':375,'raw_compile_logs':375,'recorded_unique_dependencies':len(dependencies),'compile_time_include_evidence_present':True,'current_external_include_resolution_reprobed':False,'strict_reuse_check_required_before_reuse':True,'compiler_rebuilt_by_archiver':False,'SDK_rebuilt':False,'hardware_verified':False}
def inventory(cache):
 result={}
 for p in sorted(cache.rglob('*')):
  if p.is_symlink():raise ValueError('Cache symlink is not a complete portable archive')
  if p.is_file():result['compiler/'+p.relative_to(cache).as_posix()]=sha(p)
 return result
RECOVERY=['third_party/mesa-pco/source-plan.json','tools/ci/build-ge8300-compiler-source.py','tools/ci/ge8300_compiler_cache.py']
def verify_archive(path):
 with zipfile.ZipFile(path) as z:
  names=z.namelist()
  if len(names)!=len(set(names)):raise ValueError('Duplicate ZIP member')
  data=json.loads(z.read('compiler-cache-inventory.json'))
  if data.get('schema')!='pco375-complete-recovery-archive-v1':raise ValueError('Wrong archive schema')
  expected=data['files']
  if set(names)!=set(expected)|{'compiler-cache-inventory.json'}:raise ValueError('ZIP member inventory incomplete')
  for name,h in expected.items():
   if name.startswith('/') or '..' in Path(name).parts or '\\' in name:raise ValueError('Escaped ZIP path')
   with z.open(name) as f:
    if hashlib.file_digest(f,'sha256').hexdigest()!=h:raise ValueError('ZIP content SHA mismatch: '+name)
 return {'archive_sha256':sha(path),'members_checked':len(expected),'compiled_source_commit':data['cache_proof']['compiled_source_commit'],'cache_proof':data['cache_proof']}
def archive(root,cache,expected,output):
 root=root.resolve();cache=cache.resolve();output=output.resolve();receipt=Path(str(output)+'.receipt.json');partial=Path(str(output)+'.partial')
 if not output.is_relative_to(root) or output.is_relative_to(cache) or any(p.exists() for p in [output,receipt,partial]):raise ValueError('Fresh checkout-local archive outside compiler directory required')
 proof=validate(root,cache,expected);files=inventory(cache);recovery={'recovery/'+name:sha(root/name) for name in RECOVERY};allfiles={**files,**recovery}
 record={'schema':'pco375-complete-recovery-archive-v1','cache_proof':proof,'files':allfiles,'note':'Recorded include hashes/raw depfiles preserved; SDK header binaries are not bundled; strict live include/API/tool verification remains mandatory before cache reuse'}
 output.parent.mkdir(parents=True,exist_ok=True)
 with zipfile.ZipFile(partial,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6,allowZip64=True) as z:
  for name in allfiles:
   source=cache/name[len('compiler/'):] if name.startswith('compiler/') else root/name[len('recovery/'):]
   z.write(source,name)
  z.writestr('compiler-cache-inventory.json',json.dumps(record,indent=2)+'\n')
 result=verify_archive(partial)
 if inventory(cache)!=files or any(sha(root/name)!=recovery['recovery/'+name] for name in RECOVERY):raise ValueError('Compiler cache/source changed while archiving')
 os.replace(partial,output)
 result.update(schema=record['schema'],archive=str(output),include_payloads='recorded evidence and raw dependency lists; external SDK headers not copied',success=True)
 receipt.write_text(json.dumps(result,indent=2)+'\n');return result
def main():
 p=argparse.ArgumentParser();p.add_argument('--cache',type=Path);p.add_argument('--expected-link-sha256');p.add_argument('--output',type=Path);p.add_argument('--verify-archive',type=Path);a=p.parse_args()
 if a.verify_archive:
  if any([a.cache,a.expected_link_sha256,a.output]):raise ValueError('Verify archive is a separate read-only mode')
  result=verify_archive(a.verify_archive)
 else:
  if not all([a.cache,a.expected_link_sha256,a.output]):raise ValueError('Cache, exact link manifest SHA and fresh output required')
  result=archive(ROOT,a.cache,a.expected_link_sha256.lower(),a.output)
 print(json.dumps(result))
if __name__=='__main__':main()
