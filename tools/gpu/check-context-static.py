#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compare typed Mesa context static registers against PDS component addresses."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import sys

HERE=pathlib.Path(__file__).resolve().parent

def analyze(events):
    parts={};checked=0;indices=set()
    for e in events:
        if e['event']=='ctx_component':parts[(e['kind'],e['index'])]=e
        elif e['event']=='ctx_static':
            i=e['index']
            if i in indices:raise ValueError('duplicate context slot')
            indices.add(i)
            def address(kind,index=i):return parts[(kind,index)]['gpu_va']
            if i not in (0,1) or e['vdm']!=address('vdm_state',0) or e['geom']!=address('geom_state',0):
                raise ValueError('context bases differ')
            for label,sr,pt,word in (('store','store','store',0x22001004),('resume','load','resume',0x20001004)):
                data=address('sr_'+sr+'_data');code=address('sr_'+sr+'_code')
                ptdata=address('pt_'+pt+'_data');ptcode=address('pt_'+pt+'_code')
                if (any(x>>32!=0xda or x&15 for x in (data,code,ptdata,ptcode)) or ptcode!=ptdata+32 or
                    e[label+'0']!=(((data&0xffffffff)|5)<<32)|word or
                    e[label+'1']!=(code&0xffffffff) or e[label+'2']!=((ptdata&0xffffffff)<<32)|0x420):
                    raise ValueError('context static task differs')
            checked+=1
    if checked!=2 or indices!={0,1}:raise ValueError('unexpected context count')
    return {'context_slots':checked,'gpu_execution_allowed':False,
            'limitations':['register construction alone does not prove context shader semantics or GPU lifetime']}

def check(root):
    root=pathlib.Path(root);m=json.loads((root/'run-manifest.json').read_text());raw=(root/'events.jsonl').read_bytes()
    if (not (m.get('diagnostic_manifest') or {}).get('context_static_metadata_only') or
        hashlib.sha256(raw).hexdigest()!=m.get('events_sha256')):raise ValueError('capture provenance')
    spec=importlib.util.spec_from_file_location('context',HERE/'check-render-context.py')
    ctx=importlib.util.module_from_spec(spec);spec.loader.exec_module(ctx);ctx.check(root)
    return analyze([json.loads(line) for line in raw.splitlines()])

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try:print(json.dumps(check(a.capture),indent=2))
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):
        print('invalid context static capture',file=sys.stderr);return 2
    return 1
if __name__=='__main__':sys.exit(main())
