#!/usr/bin/env python3
"""Synthetic375 recovery inventory guards. No PCO compilation or SDK install."""
import argparse,importlib.util,json,pathlib,zipfile,shutil
root=pathlib.Path(__file__).resolve().parents[1]
s=importlib.util.spec_from_file_location('archive',root/'tools/ci/archive-ge8300-compiler-cache.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
a=argparse.ArgumentParser();a.add_argument('--output',type=pathlib.Path,required=True);out=a.parse_args().output.resolve()
if out.exists():raise ValueError('Fresh fixture output required')
repo=out/'source';cache=repo/'compiler';cache.mkdir(parents=True)
def write(p,v):p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(v,indent=2)+'\n')
for name in m.RECOVERY[1:]:
 dest=repo/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(root/name,dest)
units=[];rows=[];proof=[];files=[]
for i in range(375):
 source=f'compiler-source/unit{i}.c';p=repo/source;p.parent.mkdir(exist_ok=True);p.write_text('/* synthetic source '+str(i)+' */');obj=f'{i}-unit.obj';(cache/obj).write_bytes(b'synthetic object '+str(i).encode());(cache/f'{i}.d').write_text('ge8300-unit: '+str(p)+'\n');(cache/f'{i}.log').write_text('synthetic compile log')
 unit={'source':source,'component':'pair' if i==374 else ['pco','nir','util','support'][i%4],'arguments':['--target=thumbv7-pc-windows-msvc']};units.append(unit);files.append({'path':source,'sha256':m.sha(p)})
 deps=[{'path':'${REPO}/'+source,'sha256':m.sha(p)},{'path':'${SDK_UM}/synthetic.h','sha256':'e'*64}]
 row={**unit,'exit_code':0,'source_sha256':m.sha(p),'object':obj,'object_sha256':m.sha(cache/obj),'dependencies':deps,'command':['synthetic-clang','-MD','-MF',str(cache/f'{i}.d'),'-MT','ge8300-unit','-c',str(p),'-o',str(cache/obj)]};rows.append(row);proof.append({**unit,'source_sha256':row['source_sha256'],'object_sha256':row['object_sha256'],'dependencies':deps})
write(repo/m.RECOVERY[0],{'files':files,'units':units});plan=m.sha(repo/m.RECOVERY[0]);write(cache/'compile-manifest.json',{'units':rows,'source_plan_sha256':plan})
evidence={'schema':'pco375-all-includes-cache-v1','units':proof,'compiler_sha256':'f'*64,'source_plan_sha256':plan,'compile_manifest_sha256':m.sha(cache/'compile-manifest.json'),'build_recipe_sha256':m.sha(repo/m.RECOVERY[1]),'cache_verifier_sha256':m.sha(repo/m.RECOVERY[2])};write(cache/'cache-evidence.json',evidence)
inputs=[]
for name in sorted(m.LIBRARIES):(cache/name).write_text('synthetic library '+name);inputs.append({'path':'C:\\original-VM\\compiler\\'+name,'sha256':m.sha(cache/name)})
(cache/'pco_pair.obj').write_bytes((cache/rows[-1]['object']).read_bytes());(cache/'arm-neon-vfs.json').write_text('{}')
write(cache/'pair-manifest.json',{'source_commit':'a'*40,'source_plan_sha256':plan,'source_dirty':False,'source_built':True,'user_mode_fp':True,'kernel_no_fp_applied':False,'object_sha256':rows[-1]['object_sha256'],'compiled_source_sha256':rows[-1]['source_sha256']})
write(cache/'link-manifest.json',{'schema':'compiler-source-closure-v1','source_commit':'a'*40,'source_dirty':False,'source_built':True,'all_compile_exit_codes_zero':True,'compiled_units':375,'source_plan_sha256':plan,'compiler_sha256':'f'*64,'cache_evidence_sha256':m.sha(cache/'cache-evidence.json'),'inputs':inputs});pin=m.sha(cache/'link-manifest.json')
checks=[]
for name in ['0-unit.obj','libcmt.lib','cache-evidence.json','374.d','374.log','arm-neon-vfs.json']:
 path=cache/name;saved=path.read_bytes();path.unlink()
 try:m.validate(repo,cache,pin)
 except (ValueError,FileNotFoundError):checks.append('reject missing '+name)
 else:raise AssertionError('Incomplete cache accepted: '+name)
 path.write_bytes(saved)
archive=repo/'compiler375-cache.zip';r=m.archive(repo,cache,pin,archive);assert r['cache_proof']['unique_objects']==375 and r['cache_proof']['libraries']==10 and r['cache_proof']['strict_reuse_check_required_before_reuse']
restored=repo/'restore';restored.mkdir()
with zipfile.ZipFile(archive) as z:z.extractall(restored)
assert m.validate(repo,restored/'compiler',pin)['compiled_source_commit']=='a'*40
bad=repo/'bad.zip'
with zipfile.ZipFile(archive) as old,zipfile.ZipFile(bad,'w') as z:
 for name in old.namelist():z.writestr(name,b'corrupt' if name=='compiler/0-unit.obj' else old.read(name))
try:m.verify_archive(bad)
except ValueError:checks.append('reject corrupted archived object')
else:raise AssertionError('Corrupt archive accepted')
recipe=(root/'tools/ci/build-ge8300-from-source.ps1').read_text()
assert '[string]$KernelVcToolsDirectory,[switch]$ArchiveCompilerCache' in recipe
params=recipe[:recipe.index('$ErrorActionPreference')]
assert params.index('$CompilerJobs')<params.index('$VerifiedCompilerDirectory')<params.index('$ExpectedCompilerCacheManifestSha256')<params.index('$KernelVcToolsDirectory')
assert '$scKernelVc=$scVc' in recipe and 'if($KernelVcToolsDirectory)' in recipe
assert recipe.count('-MicrosoftLinker $scKernelLink')==2
assert '-MicrosoftArmCompiler $scKernelCl' in recipe and '-MicrosoftCrtInclude "$scKernelVc\\include"' in recipe
compiler_call=next(line for line in recipe.splitlines() if '& python -B "$PSScriptRoot\\build-ge8300-compiler-source.py"' in line)
assert '$scKernelVc' not in compiler_call and '"$scVc\\lib\\arm"' in compiler_call
assert recipe.index('build-ge8300-compiler-source.py')<recipe.index('archive-ge8300-compiler-cache.py')<recipe.index('& "$PSScriptRoot\\build-ge8300-native-owner-core.ps1"')
assert "if(!$VerifiedCompilerDirectory){" in recipe and 'no fallback rebuild' in recipe
assert 'SourceCompiler -CompilerSourceCommit $scCompilerCommit' in recipe
print(json.dumps({'synthetic_only':True,'archive_restore_roundtrip':True,'objects':375,'libraries':10,'negative_checks':checks,'kernel_PCO_tool_split_contract':True,'compile_or_runner_operations':0}))
