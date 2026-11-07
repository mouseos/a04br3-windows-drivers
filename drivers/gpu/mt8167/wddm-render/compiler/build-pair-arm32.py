#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""既存ARM user-mode PCO ABI/CRT flagsで新pair TUだけをcompileする。"""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
p=argparse.ArgumentParser();p.add_argument('--reference',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();here=Path(__file__).resolve().parent;root=next(p for p in here.parents if (p/'windows-rt/AGENTS.md').exists());out=a.output.resolve();assert out.is_relative_to(root)
ref=json.loads(a.reference.read_text());u=next(x for x in ref['units'] if x['source'].endswith('/pco_sm2.c'));cmd=u['command'][:]
assert u['exit_code']==0 and sha(u['object'])==u['object_sha256'];assert not any(x in cmd for x in ['-mfpu=none','-msoft-float','/kernel']);assert '-D_MSC_VER=1900' in cmd
out.mkdir(parents=True,exist_ok=False);cmd[cmd.index('-c')+1]=str(here/'pco_pair.c');cmd[cmd.index('-o')+1]=str(out/'pco_pair.obj');env=dict(os.environ,TMPDIR=str(out),TEMP=str(out),TMP=str(out))
with (out/'build.log').open('w') as log:subprocess.run(cmd,env=env,stdout=log,stderr=subprocess.STDOUT,check=True)
text=subprocess.check_output(cmd[:-2]+['-M'],env=env,text=True);import shlex
deps={n:sha(n) for n in shlex.split(text.replace('\\\n',' '))[1:]}
(out/'manifest.json').write_text(json.dumps(dict(source_commit=subprocess.check_output(['git','-C',str(here),'rev-parse','HEAD'],text=True).strip(),source_dirty=bool(subprocess.check_output(['git','-C',str(here),'status','--porcelain'],text=True)),reference_sha256=sha(a.reference),command=cmd,dependencies=deps,object_sha256=sha(out/'pco_pair.obj'),user_mode_fp=True,kernel_no_fp_applied=False,runtime_verified=False),indent=2)+'\n')
print('PAIR_USER_MODE_ARM32_OBJECT_COMPLETE')
