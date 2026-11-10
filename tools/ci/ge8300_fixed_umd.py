#!/usr/bin/env python3
"""SHA固定aa661 UMD入力。旧compile-time外部include証明を新造しない。"""
import argparse,hashlib,json,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def main():
 p=argparse.ArgumentParser();p.add_argument('--enable-fixed-umd',action='store_true');p.add_argument('--warm-directory',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 if not a.enable_fixed_umd:raise ValueError('Explicit historical UMD opt-in required')
 spec=read(ROOT/'tools/ci/ge8300-aa661-umd-input.json')
 for name,h in spec['repository_source_include_closure'].items():
  if sha(ROOT/name)!=h:raise ValueError('UMD repository source/include changed: '+name)
 if sha(ROOT/spec['recipe'])!=spec['recipe_sha256']:raise ValueError('UMD recipe changed')
 warm=a.warm_directory.resolve();manifest=warm/'umd/build-manifest.json';dll=warm/'ge8300-umd.dll'
 if sha(manifest)!=spec['manifest_sha256'] or sha(dll)!=spec['dll_sha256']:raise ValueError('Historical UMD input SHA mismatch')
 m=read(manifest)
 if m['source_commit']!=spec['UMDSourceCommit'] or m['source_dirty'] or m['signed'] or not m['unified_display']:raise ValueError('Historical UMD identity mismatch')
 out=a.output.resolve()
 if not out.is_relative_to(ROOT) or out.exists():raise ValueError('Fresh checkout-local output required')
 if subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']):raise ValueError('Clean source required')
 out.mkdir(parents=True);shutil.copyfile(dll,out/dll.name);shutil.copyfile(manifest,out/'historical-build-manifest.json')
 receipt={'schema':spec['schema'],'current_kernel_source_commit':subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),'UMDSourceCommit':spec['UMDSourceCommit'],'CompilerSourceCommit':spec['CompilerSourceCommit'],'dll_sha256':sha(out/dll.name),'historical_manifest_sha256':sha(out/'historical-build-manifest.json'),'repository_source_include_files_checked':len(spec['repository_source_include_closure']),'recipe_identical':True,'compile_time_external_include_closure_recorded':False,'PCO_libraries_required_for_fixed_binary_input':False,'UMD_recompiled':False,'signed':False,'runtime_verified':False,'hardware_verified':False,'DWM_verified':False}
 (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt))
if __name__=='__main__':main()
