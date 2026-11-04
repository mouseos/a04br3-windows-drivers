#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify fixed persistent-state PDS constants without exporting BO bytes."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import struct
import sys

def fnv(data):
    h=14695981039346656037
    for byte in data:h=((h^byte)*1099511628211)&((1<<64)-1)
    return h
def make(state,kind):
    if state<=0 or state&15 or state>(1<<40)-128:raise ValueError('invalid state')
    if kind=='pt_store_data':
        words=[(state+off)|(num<<40)|((32+15*i)<<46) for i,(off,num) in enumerate(((0,15),(60,15),(120,2)))]
    elif kind=='pt_resume_data':
        words=[(state+off)|(num<<41)|((16+6*i)<<47) for i,(off,num) in enumerate(((0,6),(48,6),(96,4)))]
    else:raise ValueError('invalid kind')
    return struct.pack('<QQQQ',*words,0)
def make_sr(state,shader,control):
    if state<=0 or state&15 or state>(1<<40)-16432 or shader>>32!=0xe0 or shader&3 or control&~0x3fe00000000 or control&(3<<33):
        raise ValueError('invalid shared state')
    return struct.pack('<16I',state>>32,state&0xffffffff,2,0,(state+48)>>32,(state+48)&0xffffffff,
                       0x80004002,0,(shader&0xfffffffc)|(control&0xffffffff),control>>32,*([0]*6))
def check(root):
    root=pathlib.Path(root);m=json.loads((root/'run-manifest.json').read_text());raw=(root/'events.jsonl').read_bytes();diag=m.get('diagnostic_manifest') or {}
    if not diag.get('context_pt_data_hash') or not diag.get('context_sr_data_hash') or hashlib.sha256(raw).hexdigest()!=m['events_sha256']:raise ValueError('invalid provenance')
    spec=importlib.util.spec_from_file_location('context',pathlib.Path(__file__).with_name('check-render-context.py'))
    ctx=importlib.util.module_from_spec(spec);spec.loader.exec_module(ctx);ctx.check(root)
    e=[json.loads(x) for x in raw.splitlines()];hashes=[x for x in e if x['event']=='ctx_pds_data_hash']
    if len(hashes)!=4:raise ValueError('unexpected constant count')
    for item in hashes:
        matches=[c for c in e if c['event']=='ctx_component' and c['kind']==item['kind'] and c['gpu_va']==item['gpu_va'] and c['size']==32]
        if len(matches)!=1:raise ValueError('data mapping mismatch')
        states=[c for c in e if c['event']=='ctx_component' and c['kind']=='pt_state' and c['index']==matches[0]['index'] and c['gpu_va']==item['state_gpu'] and c['size']>=128]
        if len(states)!=1 or fnv(make(item['state_gpu'],item['kind']))!=item['fnv64']:raise ValueError('constant mismatch')
    shared=[x for x in e if x['event']=='ctx_sr_data_hash']
    if len(shared)!=4:raise ValueError('unexpected shared constant count')
    for item in shared:
        matches=[c for c in e if c['event']=='ctx_component' and c['kind'] in ('sr_store_data','sr_load_data') and c['gpu_va']==item['gpu_va'] and c['size']==64]
        if len(matches)!=1:raise ValueError('shared data mapping mismatch')
        c=matches[0];shader_kind=c['kind'].replace('_data','_usc')
        state=[x for x in e if x['event']=='ctx_component' and x['kind']=='sr_state' and x['index']==c['index'] and x['gpu_va']==item['state_gpu'] and x['size']>=16432]
        shader=[x for x in e if x['event']=='ctx_component' and x['kind']==shader_kind and x['index']==c['index'] and x['gpu_va']==item['shader_gpu']]
        if len(state)!=1 or len(shader)!=1 or fnv(make_sr(item['state_gpu'],item['shader_gpu'],item['control']))!=item['fnv64']:
            raise ValueError('shared constants mismatch')
    return {'verified_constant_ranges':8,'gpu_execution_allowed':False,
            'limitations':['FNV is not collision-resistant','PDS code and all other dependencies are not verified']}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try:print(json.dumps(check(a.capture),indent=2))
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):print('invalid context constants',file=sys.stderr);return 2
    return 1
if __name__=='__main__':sys.exit(main())
