#!/usr/bin/env python3
"""Warm tool guards using synthetic binary bytes and real source-plan semantics.
Does not certify any live Windows cache, execute PowerShell, or build drivers.
"""
import argparse,copy,json,sys,hashlib,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools/ci'))
import ge8300_prebuilt_compiler as m
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);out=p.parse_args().output.resolve();out.mkdir(parents=True,exist_ok=False);cache=out/'cache';cache.mkdir()
spec=copy.deepcopy(m.read(root/'tools/ci/ge8300-pco375-f4-prebuilt.json'));plan=m.read(root/'third_party/mesa-pco/source-plan.json');rows=[]
for i,u in enumerate(plan['units']):
 args=[]
 for a in u['arguments']:
  for k,v in spec['recorded_include_roots'].items():a=a.replace('${'+k+'}',v)
  args.append(a)
 obj=f'{i}.obj';(cache/obj).write_text('synthetic object '+str(i))
 rows.append({'source':u['source'],'component':u['component'],'exit_code':0,'source_sha256':m.sha(root/u['source']),'object':obj,'object_sha256':m.sha(cache/obj),'command':['clang',*args,'-ivfsoverlay','old-arm-neon-vfs.json','-c',u['source'],'-o',obj]})
api={};tools={}
for name in spec['tool_sha256']:
 path=out/name;path.write_text('synthetic tool '+name);tools[name]=path;spec['tool_sha256'][name]=m.sha(path)
for lib in spec['libraries']:
 (cache/lib['name']).write_text('synthetic library '+lib['name']);lib['sha256']=m.sha(cache/lib['name'])
 if not lib['name'].startswith('ge8300-'):
  path=out/'API'/lib['name'];path.parent.mkdir(exist_ok=True);path.write_bytes((cache/lib['name']).read_bytes());api[lib['name']]=path
pairrow=next(x for x in rows if x['component']=='pair');(cache/'pco_pair.obj').write_bytes((cache/pairrow['object']).read_bytes());spec['pair_object_sha256']=m.sha(cache/'pco_pair.obj')
comp={'source_plan_sha256':spec['source_plan_sha256'],'units':rows}
link={'source_commit':spec['compiled_source_commit'],'source_dirty':False,'source_built':True,'all_compile_exit_codes_zero':True,'compiled_units':375,'source_plan_sha256':spec['source_plan_sha256'],'compiler_sha256':spec['tool_sha256']['clang'],'inputs':[{k:v for k,v in x.items() if k!='name'} for x in spec['libraries']]}
pair={'source_commit':spec['compiled_source_commit'],'source_dirty':False,'source_plan_sha256':spec['source_plan_sha256'],'compiled_source_sha256':spec['pair_source_sha256'],'object_sha256':spec['pair_object_sha256'],'user_mode_fp':True,'kernel_no_fp_applied':False,'dependencies':{}}
def persist():
 for name,d in [('compile-manifest.json',comp),('link-manifest.json',link),('pair-manifest.json',pair)]:
  (cache/name).write_text(json.dumps(d));spec['manifest_sha256'][name]=m.sha(cache/name)
persist()
def run(ok):
 try:r=m.validate(root,cache,api,tools,spec)
 except ValueError:
  if ok:raise
  return
 if not ok:raise AssertionError('Bad pinned prebuilt input accepted')
 assert r['includeClosureRecorded'] is False and r['strict_cache_qualified'] is False
 assert r['CompilerSourceCommit']==spec['compiled_source_commit'] and r['PCO_rebuilt']==0
run(True)
# Standalone old objects are not link inputs; their absence is recorded, not a rebuild trigger.
(cache/'0.obj').rename(cache/'0.saved');result=m.validate(root,cache,api,tools,spec);assert result['live_objects_checked']==374 and result['standalone_objects_missing']==['0.obj'];(cache/'0.saved').rename(cache/'0.obj')
for path in [cache/'0.obj',cache/'pco_pair.obj',tools['clang'],tools['llvm-lib'],api['kernel32.lib'],cache/'ge8300-pco-arm32.lib']:
 data=path.read_bytes();path.write_bytes(data+b'changed');run(False);path.write_bytes(data)
# Even a re-pinned metadata fixture cannot change codegen/source/compile identity.
old=rows[0]['command'][1];rows[0]['command'][1]='--target=other';persist();run(False);rows[0]['command'][1]=old
old=rows[0]['source_sha256'];rows[0]['source_sha256']='0'*64;persist();run(False);rows[0]['source_sha256']=old
link['source_commit']='0'*40;persist();run(False);link['source_commit']=spec['compiled_source_commit'];persist();run(True)
# Canonical cold entry pin; RT/SRV candidate manifest fields change, source/strict defaults do not.
cold_default_pins={'tools/ci/build-ge8300-from-source.ps1': '89df3b9e43b01795288c5d291e3733fd43b6269b9cc983075c13820d94679e12', 'tools/ci/ge8300_compiler_cache.py': 'aa87d445072e3d35f37aaa3f7ea5ad0831f7d2cedba8cccf9ea9babfd22a44cb'}
for name,pin in cold_default_pins.items():assert m.sha(root/name)==pin,name
warm=(root/'tools/ci/build-ge8300-shared-surface-warm.ps1').read_text()
assert 'if(!$EnableCandidate -or !$EnablePinnedPrebuiltCompiler)' in warm
assert 'build-ge8300-compiler-source.py' not in warm and 'ge8300_compiler_cache.py' not in warm
assert 'includeClosureRecorded=$false' in warm and 'PCO_rebuilt=0' in warm and 'SDK_rebuilt=0' in warm
assert warm.index('--enable-pinned-prebuilt --cache')<warm.index('& "$PSScriptRoot\\build-ge8300-native-owner-core.ps1"')
for entry in ['build-ge8300-native-owner-core.ps1','build-wdk81.ps1','build-native-development-wdk81.ps1']:
 calls=[s for s in warm.splitlines() if s.lstrip().startswith('& ') and entry in s]
 assert len(calls)==1 and not any(s in calls[0] for s in ['-ReuseDirectory','-ReuseVerifiedDirectory']),entry
no_opt=subprocess.run([sys.executable,'-B',str(root/'tools/ci/ge8300_prebuilt_compiler.py')],capture_output=True,text=True);assert no_opt.returncode and 'opt-in' in no_opt.stderr
print('PASS synthetic warm prebuilt guards: exact objects/tools/API/pair/source/flags; missing include proof preserved unknown; source/strict defaults unchanged; no compiler build fallback; opt-in required')
