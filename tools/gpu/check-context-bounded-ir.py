#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check private bounded context USC generation and its optimized NIR."""
import argparse
import hashlib
import importlib.util
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile

PIN='ef11349e8e5d620a2227fac0eed17e7ed1f86428'
HERE=pathlib.Path(__file__).resolve().parent

def sha(path):return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()

def audit(log):
    reports={}
    for label,name,values,op in (
        ('store','cs_store_sr_1024_common',{0,1,2,3,4,256,1024,4096,0xfffffc00,0xffffffff},'@dma_st_shregs_pco'),
        ('load','cs_load_sr_256_common',{0,1,2,3,4,256,1024,0xffffff00},'@dma_ld_shregs_pco')):
        sections=re.findall(r'after pco_postprocess_nir:\s*shader: MESA_SHADER_COMPUTE\s*name: '+name+r'\b(.*?)\nshader ir before passes:',log,re.S)
        if len(sections)!=1:raise ValueError('unexpected NIR section')
        text=sections[0]
        found={int(x,16) for x in re.findall(r'load_const \(0x([0-9a-fA-F]+)',text)}
        if (found!=values or text.count(op)!=1 or text.count('loop {')!=1 or text.count(' uge ')!=1 or
            ' bcsel ' not in text or ' uadd64_32 ' not in text or ' ishl ' not in text or
            text.index(' uge ')>text.index(op) or text.index('loop {')>text.index(op)):
            raise ValueError('missing bounded loop or guard')
        if label=='store':
            if (text.count('@load_shared_reg_alloc_size_pco')!=1 or text.count('@dma_st_pco')!=2 or
                not re.search(r'else\s*\{[^{}]*load_const \(0xffffffff[^{}]*@dma_st_pco',text,re.S)):
                raise ValueError('missing oversize marker')
        elif text.count('@dma_ld_pco')!=1 or ' iand ' not in text or ' umin ' not in text:
            raise ValueError('missing load bound')
        reports[label]={'guard_max_blocks':256,'loop_step_dwords':1024 if label=='store' else 256}
    return reports

def check(source,build,diagnostic):
    source=pathlib.Path(source).resolve();build=pathlib.Path(build).resolve();diagnostic=pathlib.Path(diagnostic).resolve()
    git=lambda *a:subprocess.check_output(['git','-C',str(source),*a],text=True).strip()
    meta=json.loads((diagnostic/'manifest.json').read_text())
    clc=build/'src/imagination/pco/uscgen/pco_clc'
    if (git('rev-parse','HEAD')!=PIN or git('status','--porcelain','--untracked-files=all') or
        meta['mesa_commit']!=PIN or not meta.get('pco_fill_verified') or meta.get('oversize_marker')!='0xffffffff' or
        meta['pco_clc_sha256']!=sha(clc) or meta['spv_sha256']!=sha(diagnostic/'pco_usclib.spv') or
        meta['diagnostic_source_sha256']!=sha(diagnostic/'common.cl')):raise ValueError('provenance')
    spec=importlib.util.spec_from_file_location('usc',HERE/'generate-context-usc.py');usc=importlib.util.module_from_spec(spec);spec.loader.exec_module(usc)
    with tempfile.TemporaryDirectory(prefix='a04-bounded-ir-') as tmp:
        h=pathlib.Path(tmp)/'programs.h';c=pathlib.Path(tmp)/'programs.c';env=os.environ.copy()
        env['PCO_DEBUG']='no_pred_cf';env['PCO_DEBUG_PRINT']='cs,internal,nir'
        run=subprocess.run([str(clc),str(diagnostic/'pco_usclib.spv'),str(h),str(c)],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True,timeout=120)
        expected=usc.parse(diagnostic/'programs0.c')
        if usc.parse(c)!=expected:raise ValueError('debug binary differs')
        for name,filename in zip(usc.NAMES,('sr-store-bounded.bin','sr-load-bounded.bin')):
            if expected[name][1]!=(diagnostic/filename).read_bytes() or sha(diagnostic/filename)!=meta['files'][filename]['sha256']:
                raise ValueError('shader binary differs')
        return audit(run.stdout.decode('utf-8','replace'))

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('source','build','diagnostic'):p.add_argument('--'+n,required=True,type=pathlib.Path)
    a=p.parse_args()
    try:result=check(a.source,a.build,a.diagnostic)
    except (OSError,ValueError,KeyError,TypeError,subprocess.SubprocessError,json.JSONDecodeError):
        print('bounded context NIR audit failed',file=sys.stderr);return 2
    print(json.dumps({'nir':result,'pco_fill_patterns':['00','ff'],'gpu_execution_allowed':False,
       'limitations':['NIR checks do not prove machine-code semantics or the hardware special register',
                      'overflow marker retains no restorable shared state; firmware cleanup remains necessary']},indent=2))
    return 1
if __name__=='__main__':sys.exit(main())
