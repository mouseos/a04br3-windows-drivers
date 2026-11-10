#!/usr/bin/env python3
"""Conservative PCO375 cache evidence. No compile/install/network fallback."""
import argparse,hashlib,json,re,subprocess,sys
from pathlib import Path

def sha(path):
 with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(path):return json.loads(Path(path).read_text(encoding='utf-8-sig'))
def parse_make_dependencies(text):
 # Clang's Windows make output preserves path separators (C:\dir\file)
 # and escapes spaces as \ . POSIX shlex destroys those separators.
 text=re.sub(r'\\\r?\n',' ',text.lstrip('\ufeff'))
 if not text.startswith('ge8300-unit:'):raise ValueError('Unknown compiler dependency target')
 text=text[len('ge8300-unit:'):];result=[];token=[];i=0
 while i<len(text):
  c=text[i]
  if c.isspace():
   if token:result.append(''.join(token));token=[]
   i+=1;continue
  if c=='#':
   end=text.find('\n',i);i=len(text) if end<0 else end;continue
  if c=='\\' and i+1<len(text) and (text[i+1] in ' \t#'):
   token.append(text[i+1]);i+=2;continue
  if c=='$' and i+1<len(text) and text[i+1]=='$':token.append('$');i+=2;continue
  token.append(c);i+=1
 if token:result.append(''.join(token))
 return result

def dependencies(depfile):
 return parse_make_dependencies(Path(depfile).read_text(encoding='utf-8-sig'))
def canonical(path,roots):
 p=Path(path).resolve()
 if p==(roots['VC_INCLUDE']/'arm_neon.h').resolve():return '${VC_INCLUDE}/arm_neon.h'
 p=p.resolve(strict=True)
 for key,base in sorted(roots.items(),key=lambda kv:len(str(kv[1])),reverse=True):
  try:return '${'+key+'}/'+p.relative_to(base).as_posix()
  except ValueError:pass
 raise ValueError('Compiler include escaped declared roots: '+str(p))
def resolve(token,roots):
 for key,base in roots.items():
  prefix='${'+key+'}/'
  if token.startswith(prefix):
   if token=='${VC_INCLUDE}/arm_neon.h':return roots['CLANG_RESOURCE']/'include/arm_neon.h'
   p=(base/token[len(prefix):]).resolve(strict=True)
   if not p.is_relative_to(base):raise ValueError('Dependency escaped root')
   return p
 raise ValueError('Unknown dependency root: '+token)
def dependency_rows(depfile,roots):
 return [{'path':t,'sha256':sha(resolve(t,roots))} for t in sorted({canonical(p,roots) for p in dependencies(depfile)})]
def verify(root,cache,expected,roots,compiler,api,include_probe):
 root=root.resolve();cache=cache.resolve();linkpath=cache/'link-manifest.json'
 if sha(linkpath)!=expected:raise ValueError('Compiler link manifest pin mismatch')
 link=read(linkpath);pair=read(cache/'pair-manifest.json');compilepath=cache/'compile-manifest.json';comp=read(compilepath)
 evidence=read(cache/'cache-evidence.json');planpath=root/'third_party/mesa-pco/source-plan.json';plan=read(planpath)
 if link.get('schema')!='compiler-source-closure-v1' or link.get('source_dirty') or not link.get('source_built') or not link.get('all_compile_exit_codes_zero') or link.get('compiled_units')!=375:raise ValueError('Not a clean successful source-built375 cache')
 if link.get('cache_evidence_sha256')!=sha(cache/'cache-evidence.json'):raise ValueError('Missing/bad all-include cache evidence; old cache cannot be certified retroactively')
 if evidence.get('schema')!='pco375-all-includes-cache-v1' or evidence.get('compile_manifest_sha256')!=sha(compilepath):raise ValueError('Compile/include evidence mismatch')
 if evidence.get('build_recipe_sha256')!=sha(root/'tools/ci/build-ge8300-compiler-source.py') or evidence.get('cache_verifier_sha256')!=sha(root/'tools/ci/ge8300_compiler_cache.py'):raise ValueError('Compiler build recipe changed')
 if any(x.get('source_plan_sha256')!=sha(planpath) for x in [link,pair,comp,evidence]) or len(plan['units'])!=375:raise ValueError('PCO source/flags/API plan changed')
 if link.get('compiler_sha256')!=sha(compiler) or evidence.get('compiler_sha256')!=sha(compiler):raise ValueError('Compiler binary changed')
 for x in plan['files']:
  if sha(root/x['path'])!=x['sha256']:raise ValueError('Current compiler source/include changed: '+x['path'])
 rows=comp['units'];proof=evidence['units']
 if len(rows)!=375 or len(proof)!=375:raise ValueError('Incomplete compiler TU/include closure')
 for unit,row,inc in zip(plan['units'],rows,proof):
  if row.get('exit_code')!=0 or any(row.get(k)!=unit[k] or inc.get(k)!=unit[k] for k in ['source','component']):raise ValueError('Compiler TU routing mismatch')
  if inc.get('arguments')!=unit['arguments'] or inc.get('object_sha256')!=row['object_sha256'] or inc.get('source_sha256')!=sha(root/unit['source']):raise ValueError('Compiler source/codegen arguments changed')
  if sha(cache/row['object'])!=row['object_sha256']:raise ValueError('Compiler object changed')
  deps=inc.get('dependencies',[])
  if not deps or '${REPO}/'+unit['source'] not in {x['path'] for x in deps}:raise ValueError('Missing source in include closure')
  if include_probe(unit)!=deps:raise ValueError('Current compiler include resolution changed: '+unit['source'])
  for dep in deps:
   if sha(resolve(dep['path'],roots))!=dep['sha256']:raise ValueError('Compiler include changed: '+dep['path'])
 if pair.get('source_dirty') or not pair.get('source_built') or not pair.get('user_mode_fp') or pair.get('kernel_no_fp_applied') or pair.get('source_commit')!=link['source_commit']:raise ValueError('Invalid pair source/API provenance')
 pairrow=next(row for row in rows if row['component']=='pair')
 if sha(cache/'pco_pair.obj')!=pair['object_sha256'] or pair['object_sha256']!=pairrow['object_sha256'] or pair.get('compiled_source_sha256')!=pairrow['source_sha256']:raise ValueError('Pair object/API mismatch')
 required={f'ge8300-{c}-arm32.lib' for c in ['pco','nir','util','support']}|set(api)
 if len(link['inputs'])!=10 or {Path(x['path']).name for x in link['inputs']}!=required:raise ValueError('Incomplete compiler/API libraries')
 for entry in link['inputs']:
  name=Path(entry['path']).name
  if sha(cache/name)!=entry['sha256']:raise ValueError('Cached compiler/API library changed: '+name)
  if name in api and sha(api[name])!=entry['sha256']:raise ValueError('Current toolchain API library changed: '+name)
 return {'schema':'pco375-verified-reuse-v1','current_source_commit':subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),'compiled_source_commit':link['source_commit'],'source_plan_sha256':sha(planpath),'link_manifest_sha256':sha(linkpath),'pair_manifest_sha256':sha(cache/'pair-manifest.json'),'cache_evidence_sha256':sha(cache/'cache-evidence.json'),'compiled_units':375,'all_sources_includes_flags_tools_API_identical':True,'compiler_rebuilt':False,'SDK_rebuilt':False,'hardware_verified':False}
def main():
 p=argparse.ArgumentParser()
 for key in ['cache','expected-link-sha256','vc-include','ucrt-include','sdk-shared','sdk-um','vc-arm-libraries','sdk-arm-libraries','ucrt-arm-libraries','output']:p.add_argument('--'+key,required=True)
 a=p.parse_args();root=Path(__file__).resolve().parents[2]
 import shutil
 compiler=shutil.which('clang')
 if not compiler:raise ValueError('Existing matching Clang required')
 resource=Path(subprocess.check_output([compiler,'-print-resource-dir'],text=True).strip()).resolve()
 roots={'REPO':root,'CLANG_RESOURCE':resource,**{k:Path(getattr(a,k.lower())).resolve(strict=True) for k in ['VC_INCLUDE','UCRT_INCLUDE','SDK_SHARED','SDK_UM']}}
 api={n:Path(d)/n for d,names in [(a.vc_arm_libraries,['libcmt.lib','libvcruntime.lib','libcpmt.lib']),(a.sdk_arm_libraries,['kernel32.lib','synchronization.lib']),(a.ucrt_arm_libraries,['libucrt.lib'])] for n in names}
 output=Path(a.output).resolve()
 if not output.is_relative_to(root):raise ValueError('Receipt must remain checkout-local')
 # Re-resolve all includes with today's search paths; added shadow headers must
 # not bypass equality merely because every OLD dependency still hashes alike.
 import tempfile
 with tempfile.TemporaryDirectory(prefix='pco375-includes-',dir=output.parent) as td:
  td=Path(td);vfs=td/'vfs.json'
  vfs.write_text(json.dumps({'version':0,'case-sensitive':'false','roots':[{'type':'file','name':(roots['VC_INCLUDE']/'arm_neon.h').as_posix(),'external-contents':(resource/'include/arm_neon.h').as_posix()}]}))
  def probe(unit):
   args=[]
   for argument in unit['arguments']:
    for key,value in roots.items():argument=argument.replace('${'+key+'}',str(value))
    if '${' in argument:raise ValueError('Unexpanded compiler flag')
    args.append(argument)
   depfile=td/'current.d'
   subprocess.run([compiler,*args,'-ivfsoverlay',str(vfs),'-M','-MF',str(depfile),'-MT','ge8300-unit',str(root/unit['source'])],check=True,stdout=subprocess.DEVNULL)
   return dependency_rows(depfile,roots)
  result=verify(root,Path(a.cache),a.expected_link_sha256.lower(),roots,compiler,api,probe)
 output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
if __name__=='__main__':main()
