#!/usr/bin/env python3
"""Synthetic cache-proof regressions; never claims actual PCO binary reuse."""
import argparse,copy,hashlib,json,sys
from pathlib import Path
from unittest.mock import patch
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools/ci'))
import ge8300_compiler_cache as m
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);out=p.parse_args().output.resolve()
if out.exists():raise ValueError('Fresh host fixture directory required')
out.mkdir(parents=True);repo=out/'source';cache=out/'cache';cache.mkdir();(repo/'tools/ci').mkdir(parents=True)
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
for name in ['build-ge8300-compiler-source.py','ge8300_compiler_cache.py']:(repo/'tools/ci'/name).write_bytes((root/'tools/ci'/name).read_bytes())
compiler=out/'clang-fixture';compiler.write_text('mock compiler identity')
roots={'REPO':repo}
for key in ['VC_INCLUDE','UCRT_INCLUDE','SDK_SHARED','SDK_UM','CLANG_RESOURCE']:
 roots[key]=out/key;roots[key].mkdir();(roots[key]/'api.h').write_text('fixed API '+key)
units=[];rows=[];proof=[];files=[]
for i in range(375):
 source=f'compiler/unit{i}.c';path=repo/source;path.parent.mkdir(exist_ok=True);path.write_text('/* fixed source '+str(i)+' */')
 unit={'source':source,'component':'pair' if i==374 else ['pco','nir','util','support'][i%4],'arguments':['--target=armv7-w64-windows-gnu','-I${SDK_UM}']};units.append(unit);files.append({'path':source,'sha256':m.sha(path)})
 obj=f'{i}.obj';(cache/obj).write_text('object '+str(i));row={**unit,'source_sha256':m.sha(path),'object':obj,'object_sha256':m.sha(cache/obj),'exit_code':0};rows.append(row)
 deps=[{'path':'${REPO}/'+source,'sha256':m.sha(path)},{'path':'${SDK_UM}/api.h','sha256':m.sha(roots['SDK_UM']/'api.h')}]
 proof.append({**unit,'source_sha256':row['source_sha256'],'object_sha256':row['object_sha256'],'dependencies':deps})
planpath=repo/'third_party/mesa-pco/source-plan.json';write(planpath,{'files':files,'units':units});planhash=m.sha(planpath)
write(cache/'compile-manifest.json',{'units':rows,'source_plan_sha256':planhash})
evidence={'schema':'pco375-all-includes-cache-v1','source_plan_sha256':planhash,'compiler_sha256':m.sha(compiler),'build_recipe_sha256':m.sha(repo/'tools/ci/build-ge8300-compiler-source.py'),'cache_verifier_sha256':m.sha(repo/'tools/ci/ge8300_compiler_cache.py'),'compile_manifest_sha256':m.sha(cache/'compile-manifest.json'),'units':proof};write(cache/'cache-evidence.json',evidence)
(cache/'pco_pair.obj').write_bytes((cache/'374.obj').read_bytes());write(cache/'pair-manifest.json',{'source_commit':'a'*40,'source_dirty':False,'source_built':True,'user_mode_fp':True,'kernel_no_fp_applied':False,'source_plan_sha256':planhash,'object_sha256':m.sha(cache/'pco_pair.obj'),'compiled_source_sha256':rows[-1]['source_sha256']})
api={};inputs=[]
for name in [f'ge8300-{c}-arm32.lib' for c in ['pco','nir','util','support']]+['libcmt.lib','libvcruntime.lib','libcpmt.lib','libucrt.lib','kernel32.lib','synchronization.lib']:
 (cache/name).write_text('library '+name);inputs.append({'path':str(cache/name),'sha256':m.sha(cache/name)})
 if not name.startswith('ge8300-'):
  a=out/'api-libs'/name;a.parent.mkdir(exist_ok=True);a.write_bytes((cache/name).read_bytes());api[name]=a
link={'schema':'compiler-source-closure-v1','source_commit':'a'*40,'source_dirty':False,'source_built':True,'all_compile_exit_codes_zero':True,'compiled_units':375,'compiler_sha256':m.sha(compiler),'source_plan_sha256':planhash,'cache_evidence_sha256':m.sha(cache/'cache-evidence.json'),'inputs':inputs};write(cache/'link-manifest.json',link);pin=m.sha(cache/'link-manifest.json')
by_source={x['source']:x['dependencies'] for x in proof}
def probe(u):return by_source[u['source']]
def run(ok,probe_fn=probe):
 with patch.object(m.subprocess,'check_output',return_value='b'*40+'\n'):
  try:result=m.verify(repo,cache,pin,roots,compiler,api,probe_fn)
  except (ValueError,FileNotFoundError):
   if ok:raise
   return
  if not ok:raise AssertionError('Invalid cache accepted')
  assert result['all_sources_includes_flags_tools_API_identical']
run(True)
for path in [roots['SDK_UM']/'api.h',compiler,api['kernel32.lib'],cache/'0.obj',repo/units[0]['source'],cache/'pco_pair.obj']:
 old=path.read_bytes();path.write_bytes(old+b'changed');run(False);path.write_bytes(old)
run(False,lambda u:[])
(cache/'cache-evidence.json').rename(cache/'cache-evidence.missing');run(False);(cache/'cache-evidence.missing').rename(cache/'cache-evidence.json')
# Reject a re-pinned object proof whose compilation arguments differ from plan.
bad=copy.deepcopy(evidence);bad['units'][0]['arguments']=['-O0'];write(cache/'cache-evidence.json',bad);link['cache_evidence_sha256']=m.sha(cache/'cache-evidence.json');write(cache/'link-manifest.json',link);pin=m.sha(cache/'link-manifest.json');run(False)
write(cache/'cache-evidence.json',evidence);link['cache_evidence_sha256']=m.sha(cache/'cache-evidence.json');write(cache/'link-manifest.json',link);pin=m.sha(cache/'link-manifest.json');run(True)
dep=out/'paths.d';dep.write_text('ge8300-unit: C:/source/a.c C:/Program\\ Files/sdk/api.h \\\n C:/sdk/header.h\n');assert m.dependencies(dep)==['C:/source/a.c','C:/Program Files/sdk/api.h','C:/sdk/header.h']
# VFS can provide arm_neon.h even when the physical VC include file is absent.
(roots['CLANG_RESOURCE']/'include').mkdir();neon=roots['CLANG_RESOURCE']/'include/arm_neon.h';neon.write_text('actual clang neon')
assert m.canonical(roots['VC_INCLUDE']/'arm_neon.h',roots)=='${VC_INCLUDE}/arm_neon.h'
assert m.resolve('${VC_INCLUDE}/arm_neon.h',roots)==neon
print('PASS synthetic375 cache: full proof accepted; source/include/shadow-resolution/flags/compiler/API/object/pair/missing-evidence mutations rejected; escaped dependency paths parsed')
