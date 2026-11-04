#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Audit fixed context shader NIR immediates in a private compiler run."""
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
def audit(log):
    result={}
    for label,name,expected,ops in (
        ('store','cs_store_sr_1024_common',{0,1,2,3,4,1024,4096,0xfffffc00},('@load_shared_reg_alloc_size_pco','@dma_st_pco','@dma_st_shregs_pco')),
        ('load','cs_load_sr_256_common',{0,1,2,3,4,256,1024,0xffffff00},('@dma_ld_pco','@dma_ld_shregs_pco'))):
        match=re.findall(r'after pco_postprocess_nir:\s*shader: MESA_SHADER_COMPUTE\s*name: '+name+r'\b(.*?)\nshader ir before passes:',log,re.S)
        if len(match)!=1:raise ValueError('unexpected NIR section')
        text=match[0]
        values={int(x,16) for x in re.findall(r'load_const \(0x([0-9a-fA-F]+)',text)}
        if values!=expected or any(text.count(op)!=1 for op in ops) or text.count('loop {')!=1:
            raise ValueError('unexpected instructions or constants')
        for op in ('ishl','iadd','ieq','uadd64_32'):
            if op not in text:raise ValueError('missing loop operation')
        result[label]={'nir_constants':sorted(values),'state_address_source':'PDS vertex input registers',
                       'size_source':'special register 103' if label=='store' else 'GPU-written state word',
                       'memory_ops':len(ops)-1 if label=='store' else len(ops)}
    return result
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('source','build'):p.add_argument('--'+n,required=True,type=pathlib.Path)
    a=p.parse_args();src=a.source.resolve();build=a.build.resolve()
    git=lambda *x:subprocess.check_output(['git','-C',str(src),*x],text=True).strip()
    try:
        if git('rev-parse','HEAD')!=PIN or git('status','--porcelain','--untracked-files=all'):raise ValueError('source mismatch')
        clc=build/'src/imagination/pco/uscgen/pco_clc';spv=build/'src/imagination/pco/usclib/pco_usclib.spv'
        baseline=build/'src/imagination/pco/uscgen/pco_uscgen_programs.c'
        spec=importlib.util.spec_from_file_location('usc',HERE/'generate-context-usc.py');usc=importlib.util.module_from_spec(spec);spec.loader.exec_module(usc)
        with tempfile.TemporaryDirectory(prefix='a04-usc-ir-') as td:
            c=pathlib.Path(td)/'out.c';h=pathlib.Path(td)/'out.h';env=os.environ.copy()
            env['PCO_DEBUG']='no_pred_cf';env['PCO_DEBUG_PRINT']='cs,internal,nir'
            run=subprocess.run([str(clc),str(spv),str(h),str(c)],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True,timeout=120)
            if usc.parse(c)!=usc.parse(baseline):raise ValueError('debug build differs')
            result=audit(run.stdout.decode('utf-8','replace'))
        print(json.dumps({'mesa_commit':PIN,'pco_sha256':hashlib.sha256(clc.read_bytes()).hexdigest(),'nir_audit':result,
          'gpu_execution_allowed':False,'limitations':['NIR constants are not a complete USC machine-code immediate audit',
          'special register hardware range, GPU-generated state and dynamic addresses are unverified',
          'source-level loops can underflow on non-aligned sizes']},indent=2))
    except (OSError,ValueError,KeyError,subprocess.SubprocessError):
        print('context USC IR audit failed',file=sys.stderr);return 2
    return 1
if __name__=='__main__':sys.exit(main())
