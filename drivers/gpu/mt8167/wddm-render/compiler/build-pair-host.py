#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""固定E PCO参照閉包を再利用してruntimeと同じpair compilerの実artifactを生成。"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True);p.add_argument('--tokens',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
here=Path(__file__).resolve().parent;root=next(p for p in here.parents if (p/'windows-rt/AGENTS.md').exists());out=a.output.resolve()
assert out.is_relative_to(root);out.mkdir(parents=True,exist_ok=False);ref=json.loads(a.reference.read_text())
for path,digest in ref['reference_linked_objects'].items():assert sha(path)==digest,path
build=root/'unixnu/UniXNU-a04br3/.cache/build-pvr-hostcheck';env=dict(os.environ,TMPDIR=str(out),LD_LIBRARY_PATH=str(root/'unixnu/UniXNU-a04br3/.cache/pvr-host-env/lib'),MESA_SHADER_CACHE_DISABLE='true')
commands=[];objects=[]
with (out/'build.log').open('w') as log:
 for unit in ['sm2','sm4','pco_sm2','pco_pair','pair_offline']:
  c=ref['compile'][:];c[c.index('-c')+1]=str(here/(unit+'.c'));obj=out/(unit+'.o');c[c.index('-o')+1]=str(obj);commands.append(c);subprocess.run(c,cwd=build,env=env,stdout=log,stderr=subprocess.STDOUT,check=True);objects.append(str(obj))
 c=ref['link'][:];c[c.index('-o')+1]=str(out/'pair-compiler');i=c.index('-Wl,--start-group')-1;c[i:i+1]=objects;commands.append(c);subprocess.run(c,cwd=build,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
with (out/'compiler-output.log').open('w') as log:subprocess.run([str(out/'pair-compiler'),str(a.tokens.resolve()/'vs.tokens.bin'),str(a.tokens.resolve()/'fs.tokens.bin'),str(out)],cwd=build,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
for n in ['vs.usc','fs.usc']:assert (out/n).read_bytes()==(a.tokens/n).read_bytes(),'E native admission differs: '+n
(out/'manifest.json').write_text(json.dumps(dict(source_commit=subprocess.check_output(['git','-C',str(here),'rev-parse','HEAD'],text=True).strip(),source_dirty=bool(subprocess.check_output(['git','-C',str(here),'status','--porcelain'],text=True)),reference_sha256=sha(a.reference),sources={p.name:sha(p) for p in here.glob('*') if p.suffix in ['.c','.h']},commands=commands,outputs={n:sha(out/n) for n in ['vs.usc','fs.usc','pair-link.bin']},native_admission_bytes_equal=True,gpu_executed=False),indent=2)+'\n')
print('PAIR_COMPILER_ARTIFACT_MATCHES_NATIVE_ADMISSION')
