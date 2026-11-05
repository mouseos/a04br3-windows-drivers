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
    p.add_argument('--compiler-diagnostic',type=pathlib.Path,
                   help='independent PCO fill-pattern diagnostic')
    a=p.parse_args();src,build,out=(x.resolve() for x in (a.source,a.build,a.output))
    git=lambda *x:subprocess.check_output(['git','-C',str(src),*x],text=True).strip()
    if git('rev-parse','HEAD')!=PIN or git('status','--porcelain','--untracked-files=all'):raise SystemExit('Mesa source mismatch')
    original=src/'src/imagination/pco/usclib/common.cl';text=original.read_text()
    marker='''   /* Store the allocation size in 16 dword blocks. */'''
    if text.count(marker)!=1 or text.count('         size_remaining_dw -= block_size_dw;')!=1:raise SystemExit('unexpected shared routine')
    text=text.replace(marker,'''   /* Diagnostic bound: size is in 16-dword blocks, state holds 4096 dwords.
    * Preserve an explicit failure marker instead of silently restoring an
    * empty state. A corrupted saved size is rejected on load as well. */
   if (alloc_size_16dw > 256) {
      if (store)
         nir_dma_st_pco(true, size_addr, 0xffffffffu);
      return;
   }

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
    fill_verified=False
    if a.compiler_diagnostic:
        diag=a.compiler_diagnostic.resolve();meta=json.loads((diag/'diagnostic-manifest.json').read_text())
        archive=diag/'libpowervr_compiler.a'
        if (meta.get('mesa_commit')!=PIN or not meta.get('usc_compiler_metadata_only') or
            meta.get('base_build_ninja_sha256')!=sha(build/'build.ninja') or
            sha(diag/'pco_binary.c.o')!=meta.get('pco_object_sha256')):
            raise SystemExit('compiler diagnostic mismatch')
        original_members=subprocess.check_output(['ar','t',str(build/'src/imagination/pco/libpowervr_compiler.a')],text=True).splitlines()
        diag_members=subprocess.check_output(['ar','t',str(archive)],text=True).splitlines()
        if len(original_members)!=len(diag_members):raise SystemExit('compiler archive mismatch')
        for old,new in zip(original_members,diag_members):
            expected=diag/'pco_binary.c.o' if old.endswith('/pco_binary.c.o') else (build/old).resolve()
            if pathlib.Path(new).resolve()!=expected.resolve():raise SystemExit('compiler archive member mismatch')
        cmds=subprocess.check_output(['ninja','-C',str(build),'-t','commands','src/imagination/pco/uscgen/pco_clc'],text=True).splitlines()
        link=shlex.split(next(x for x in cmds if x.startswith('c++  -o src/imagination/pco/uscgen/pco_clc ')))
        link[link.index('-o')+1]=str(out/'pco_clc_diagnostic')
        link=[str(archive) if x=='src/imagination/pco/libpowervr_compiler.a' else x for x in link]
        subprocess.run(link,cwd=build,check=True,timeout=120)
        for fill in ('00','ff'):
            pattern_env=env.copy();pattern_env['A04_PCO_FILL']=fill
            pattern_env['A04_CAPTURE_JSONL']=str(out/('pco-'+fill+'.jsonl'))
            pattern_env['A04_PCO_CANDIDATE_DIR']=str(out)
            subprocess.run([str(out/'pco_clc_diagnostic'),str(out/'pco_usclib.spv'),
                            str(out/('fill-'+fill+'.h')),str(out/('fill-'+fill+'.c'))],
                           env=pattern_env,cwd=build,check=True,timeout=120)
            if usc.parse(out/('fill-'+fill+'.c'))!=generated:
                raise SystemExit('bounded USC differs with PCO fill pattern')
        fill_verified=True
    files={}
    for name,filename in zip(usc.NAMES,('sr-store-bounded.bin','sr-load-bounded.bin')):
        data=generated[name][1];(out/filename).write_bytes(data)
        files[filename]={'size':len(data),'sha256':hashlib.sha256(data).hexdigest(),'metadata_words':generated[name][0]}
    (out/'manifest.json').write_text(json.dumps({'mesa_commit':PIN,'original_sha256':sha(original),'diagnostic_source_sha256':sha(modified),
        'mesa_clc_sha256':sha(args[0]),'pco_clc_sha256':sha(clc),'spv_sha256':sha(out/'pco_usclib.spv'),
        'programs_sha256':sha(out/'programs0.c'),'files':files,'gpu_execution_allowed':False,
        'oversize_marker':'0xffffffff','pco_fill_verified':fill_verified,
        'limitations':['diagnostic source differs from Mesa','USC code and indirect references not independently verified',
                       'not linked into driver','no hardware validation']},indent=2)+'\n')
    print('private bounded shared-state diagnostic compiled; GPU execution prohibited')
if __name__=='__main__':main()
