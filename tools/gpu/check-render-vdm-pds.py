#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Correlate fixed probe VDM PDS fetch sizes and observed write extents."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import re
import sys

def analyze(events,log,scratchless=False):
    def addresses(name):
        return [int(x,16) for x in re.findall(r'^\s*'+name+r'\s*:\s*(0x[0-9a-fA-F]+)',log,re.M)]
    def sizes(name):
        return [int(x) for x in re.findall(r'^\s*'+name+r'\s*:\s*(\d+) bytes',log,re.M)]
    v=addresses('vs_pds_data_base_addr');d=addresses('pds_data_addr')
    vc=addresses('vs_pds_code_base_addr');dc=addresses('pds_code_addr')
    descriptor_fetch=16 if scratchless else 32;descriptor_write=12 if scratchless else 24
    if not all(len(x)==2 for x in (v,d,vc,dc)) or sizes('vs_pds_data_size')!=[16,16] or sizes('pds_data_size')!=[descriptor_fetch]*2:
        raise ValueError('unexpected fixed-probe VDM layout')
    expected={(addr,kind):(length,fetch) for addrs,kind,length,fetch in
              ((v,'vertex_attributes',12,16),(d,'descriptor_mappings',descriptor_write,descriptor_fetch)) for addr in addrs}
    if len(expected)!=4:raise ValueError('duplicate data references')
    maps={};seen=set()
    for e in events:
        op=e.get('event')
        if op=='vm_map':maps[(e['vm'],e['va'])]=e['size']
        elif op=='vm_unmap':maps.pop((e['vm'],e['va']))
        elif op=='vm_destroy':maps={k:n for k,n in maps.items() if k[0]!=e['handle']}
        elif op=='pds_construct' and (e['gpu_va'],e['kind']) in expected:
            key=(e['gpu_va'],e['kind']);length,fetch=expected[key];addr=key[0]
            if key in seen or e['size']!=length or sum(1 for (_,base),n in maps.items() if addr>=base and fetch<=n and addr-base<=n-fetch)!=1:
                raise ValueError('PDS fetch range not uniquely mapped at construction')
            seen.add(key)
    if len(seen)!=4:raise ValueError('missing PDS construction')
    code_sizes=[]
    for addr in (vc[0],dc[0],vc[1],dc[1]):
        rows=[e for e in events if e.get('event')=='upload' and e['kind']=='pds_code' and e['gpu_va']==addr]
        if len(rows)!=1 or rows[0]['size']<=0:raise ValueError('PDS code upload not identified')
        code_sizes.append(rows[0]['size'])
    return {'draws':2,'vertex_data_written':12,'vertex_data_fetch':16,
            'descriptor_data_written':descriptor_write,'descriptor_data_fetch':descriptor_fetch,'pds_code_upload_sizes':code_sizes,
            'gpu_execution_allowed':False,
            'limitations':['code upload sizes do not prove instruction execution bounds',
                           'PPP and VDM address ordering is specific to the fixed probe',
                           'full indirect references and job-time lifetimes remain unverified']}

def check(root):
    root=pathlib.Path(root);m=json.loads((root/'run-manifest.json').read_text());diag=m.get('diagnostic_manifest') or {}
    events=(root/'events.jsonl').read_bytes();log=(root/'probe.log').read_bytes()
    if not m.get('control_stream_dump') or not diag.get('pds_construct_metadata_only') or not diag.get('sipf1_patch'):
        raise ValueError('missing instrumentation')
    if hashlib.sha256(events).hexdigest()!=m['events_sha256'] or hashlib.sha256(log).hexdigest()!=m['probe_log_sha256']:
        raise ValueError('capture digest mismatch')
    spec=importlib.util.spec_from_file_location('trace',pathlib.Path(__file__).with_name('check-render-trace.py'))
    trace=importlib.util.module_from_spec(spec);spec.loader.exec_module(trace);r=trace.check(root/'events.jsonl')
    if r['uploads_unmapped_at_observation'] or r['uploads_ambiguous_vm'] or r['pds_dependency_unmapped']:
        raise ValueError('unmapped PDS')
    variant=m.get('scratchless_vertex')
    if variant and variant.get('source_sha256')!=hashlib.sha256((pathlib.Path(__file__).with_name('shaders')/'triangle-scratchless.vert').read_bytes()).hexdigest():
        raise ValueError('scratchless source mismatch')
    return analyze([json.loads(x) for x in events.splitlines()],log.decode('utf-8','replace'),bool(variant))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try: print(json.dumps(check(a.capture),indent=2))
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):
        print('invalid or unsupported VDM capture',file=sys.stderr);return 2
    return 1
if __name__=='__main__':sys.exit(main())
