#!/usr/bin/env python3
"""実Git checkoutでWindows改行設定とpinned SHAの一致を検査する。"""
import argparse,hashlib,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve()
if not out.is_relative_to(root) or out.exists():raise ValueError('Fresh checkout-local output required')
def read(n):return json.loads((root/n).read_text())
o=read('tools/ci/ge8300-fullsource-origins.json');n=read('tools/ci/ge8300-native-source-map.json');plan=read('third_party/mesa-pco/source-plan.json');expected={}
for row in o['files']+o['legacy_files_preserved']+plan['files']+[{'path':r['destination'],'sha256':r['sha256']} for r in n['files']]:
 if row['path'] in expected and expected[row['path']]!=row['sha256']:raise ValueError('Contradictory pin')
 expected[row['path']]=row['sha256']
pinned=len(expected)
for name in ['tools/ci/build-ge8300-compiler-source.py','tools/ci/ge8300_compiler_cache.py','tools/ci/build-ge8300-from-source.ps1']:expected[name]=hashlib.sha256(subprocess.check_output(['git','-C',str(root),'show','HEAD:'+name])).hexdigest()
out.mkdir(parents=True);results=[]
for autocrlf,eol in [('true','crlf'),('false','lf')]:
 d=out/autocrlf
 subprocess.run(['git','clone','--quiet','--shared','--no-checkout',str(root),str(d)],check=True)
 subprocess.run(['git','-C',str(d),'config','core.autocrlf',autocrlf],check=True);subprocess.run(['git','-C',str(d),'config','core.eol',eol],check=True)
 subprocess.run(['git','-C',str(d),'checkout','--quiet','--detach','HEAD'],check=True)
 for name,h in expected.items():
  if hashlib.sha256((d/name).read_bytes()).hexdigest()!=h:raise ValueError('Checkout byte mismatch: '+autocrlf+':'+name)
 results.append({'core.autocrlf':autocrlf,'core.eol':eol,'manifest_pinned_paths':pinned,'extra_recipe_paths':len(expected)-pinned,'all_SHA_matched':True})
receipt={'cases':results,'driver_or_compiler_build_executed':False};(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt))
