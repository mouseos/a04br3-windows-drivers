#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Make private, non-executable triangle USC candidates from Mesa compiler output."""
import argparse
import hashlib
import importlib.util
import json
import os
import pathlib

HERE=pathlib.Path(__file__).resolve().parent

def generate(ir,ones,out):
    ir=pathlib.Path(ir);out=pathlib.Path(out)
    spec=importlib.util.spec_from_file_location('audit',HERE/'audit-triangle-nir.py')
    audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
    report=audit.check(ir,ones)
    sources=ir/'private-compiler-candidates'
    capture=json.loads((ir/'run-manifest.json').read_text())
    pieces={}
    vsize,vpadded=(176,256) if capture.get('scratchless_vertex') else (312,384)
    for src,name,size,padded in (('0-0.bin','vertex-usc.bin',vsize,vpadded),('4-1.bin','fragment-usc.bin',360,448)):
        data=(sources/src).read_bytes()
        if len(data)!=size or hashlib.sha256(data).hexdigest()!=capture['private_compiler_sha256'][src]:
            raise ValueError('unexpected candidate size or digest')
        pieces[name]=data+bytes(padded-size)
    out.mkdir(mode=0o700,parents=False,exist_ok=False)
    def write(name,data):
        fd=os.open(out/name,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)
        with os.fdopen(fd,'wb') as f:f.write(data)
    for name,data in pieces.items():write(name,data)
    result={'mesa_commit':capture['mesa_build_manifest']['mesa_commit'],
      'probe_commit':capture['probe_commit'],'gpu_execution_allowed':False,
      'files':{k:{'bytes':len(v),'sha256':hashlib.sha256(v).hexdigest()} for k,v in pieces.items()},
      'nir_report':report,'limitations':['private candidate; machine code immediate provenance is not fully proven',
      'zero padding is not proof of GPU instruction bounds','VDM/PPP, shared state and all indirect references remain unresolved']}
    write('manifest.json',(json.dumps(result,indent=2)+'\n').encode())
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--ir',required=True);p.add_argument('--ones',required=True);p.add_argument('--output',required=True)
    p.add_argument('--experimental-incomplete',action='store_true');a=p.parse_args()
    if not a.experimental_incomplete:raise SystemExit('explicit offline opt-in required')
    try:generate(a.ir,a.ones,a.output)
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):raise SystemExit('private candidate generation failed')
    print('private compiler candidates generated; GPU execution prohibited')
if __name__=='__main__':main()
