#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compile a private bounded shared-state diagnostic, never a GPU fixture."""
import argparse
import hashlib
import json
import os
import pathlib
import shlex
import subprocess
PIN='ef11349e8e5d620a2227fac0eed17e7ed1f86428'
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('source','build','output'):p.add_argument('--'+n,type=pathlib.Path,required=True)
    a=p.parse_args();src,build,out=(x.resolve() for x in (a.source,a.build,a.output))
    git=lambda *x:subprocess.check_output(['git','-C',str(src),*x],text=True).strip()
    if git('rev-parse','HEAD')!=PIN or git('status','--porcelain','--untracked-files=all'):raise SystemExit('Mesa source mismatch')
    original=src/'src/imagination/pco/usclib/common.cl';text=original.read_text()
    marker='''   /* Store the allocation size in 16 dword blocks. */'''
    if text.count(marker)!=1 or text.count('         size_remaining_dw -= block_size_dw;')!=1:raise SystemExit('unexpected shared routine')
    text=text.replace(marker,'''   /* Diagnostic bound: size is in 16-dword blocks, state holds 4096 dwords. */
   if (alloc_size_16dw > 256)
      alloc_size_16dw = 0;

'''+marker)
    text=text.replace('         size_remaining_dw -= block_size_dw;','         size_remaining_dw = size_remaining_dw > block_size_dw ? size_remaining_dw - block_size_dw : 0;')
    # The original include resolves relative to the Mesa source directory;
    # the temporary source must name that same header explicitly.
    text=text.replace('#include "libcl.h"','#include "'+str(src/'src/imagination/pco/usclib/libcl.h')+'"',1)
    out.mkdir(mode=0o700,parents=True,exist_ok=False);modified=out/'common.cl';modified.write_text(text)
    commands=subprocess.check_output(['ninja','-C',str(build),'-t','commands','src/imagination/pco/usclib/pco_usclib.spv'],text=True).splitlines()
    args=shlex.split(commands[-1]);args[args.index('-o')+1]=str(out/'pco_usclib.spv');args[args.index('--depfile')+1]=str(out/'pco_usclib.d')
    matches=[i for i,x in enumerate(args) if x.endswith('/pco/usclib/common.cl')]
    if len(matches)!=1:raise SystemExit('unexpected compiler command')
    args[matches[0]]=str(modified);subprocess.run(args,cwd=build,check=True,timeout=120)
    env=os.environ.copy();env['PCO_DEBUG']='no_pred_cf';env.pop('PCO_DEBUG_PRINT',None)
    clc=build/'src/imagination/pco/uscgen/pco_clc'
    for n in range(2):
        subprocess.run([str(clc),str(out/'pco_usclib.spv'),str(out/('programs%d.h'%n)),str(out/('programs%d.c'%n))],env=env,cwd=build,check=True,timeout=120)
    import importlib.util
    spec=importlib.util.spec_from_file_location('usc',pathlib.Path(__file__).with_name('generate-context-usc.py'))
    usc=importlib.util.module_from_spec(spec);spec.loader.exec_module(usc)
    generated=usc.parse(out/'programs0.c')
    if generated!=usc.parse(out/'programs1.c'):raise SystemExit('bounded USC compiler output differs')
    files={}
    for name,filename in zip(usc.NAMES,('sr-store-bounded.bin','sr-load-bounded.bin')):
        data=generated[name][1];(out/filename).write_bytes(data)
        files[filename]={'size':len(data),'sha256':hashlib.sha256(data).hexdigest(),'metadata_words':generated[name][0]}
    (out/'manifest.json').write_text(json.dumps({'mesa_commit':PIN,'original_sha256':sha(original),'diagnostic_source_sha256':sha(modified),
        'mesa_clc_sha256':sha(args[0]),'pco_clc_sha256':sha(clc),'spv_sha256':sha(out/'pco_usclib.spv'),
        'programs_sha256':sha(out/'programs0.c'),'files':files,'gpu_execution_allowed':False,
        'limitations':['diagnostic source differs from Mesa','USC code and indirect references not independently verified',
                       'not linked into driver','no hardware validation']},indent=2)+'\n')
    print('private bounded shared-state diagnostic compiled; GPU execution prohibited')
if __name__=='__main__':main()
