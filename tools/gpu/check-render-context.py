#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Trace Mesa context-switch allocations and live upload extents."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import sys

KINDS=('pt_state','sr_state','sr_store_usc','sr_load_usc','pt_store_data','pt_store_code',
       'pt_resume_data','pt_resume_code','sr_store_data','sr_store_code','sr_load_data','sr_load_code')
EXPECTED={(k,i) for k in KINDS for i in (0,1)} | {('vdm_state',0),('geom_state',0)}
LIMIT=1<<40

def analyze(events):
    maps={};uploads=[];components={};ctx=None;vm=None;jobs=0
    def mapped(addr,size,vm_id=None):
        return [k for k,n in maps.items() if (vm_id is None or k[0]==vm_id) and addr>=k[1] and size<=n and addr-k[1]<=n-size]
    def uploaded(component,vm_id):
        kind=component['kind']; expected='usc_code' if kind.endswith('_usc') else 'pds_code' if kind.endswith('_code') else 'pds_data' if kind.endswith('_data') else None
        if expected is None:return True
        return sum(1 for u in uploads if u['vm']==vm_id and u['kind']==expected and u['va']==component['gpu_va'] and 0<u['size']<=component['size'])==1
    for e in events:
        op=e['event']
        if op=='vm_map':maps[(e['vm'],e['va'])]=e['size']
        elif op=='vm_unmap':
            maps.pop((e['vm'],e['va']))
            uploads=[u for u in uploads if u['map']!=(e['vm'],e['va'])]
        elif op=='vm_destroy':
            maps={k:n for k,n in maps.items() if k[0]!=e['handle']};uploads=[u for u in uploads if u['vm']!=e['handle']]
        elif op in ('upload','pds_construct'):
            addr=e['gpu_va'];size=e['size'];matches=mapped(addr,size)
            if len(matches)!=1:raise ValueError('ambiguous upload')
            k=matches[0];uploads=[u for u in uploads if u['vm']!=k[0] or not (addr<u['va']+u['size'] and u['va']<addr+size)]
            uploads.append(dict(vm=k[0],map=k,va=addr,size=size,kind=e['kind']))
        elif op=='ctx_component':
            key=(e['kind'],e['index']);addr=e['gpu_va'];size=e['size']
            if key not in EXPECTED or key in components or addr<=0 or addr>=LIMIT or size<=0 or size>LIMIT-addr or len(mapped(addr,size))!=1:
                raise ValueError('invalid context component')
            components[key]=e
        elif op=='context_create' and e['type']==0:
            if ctx is not None or set(components)!=EXPECTED:raise ValueError('unexpected render context')
            ctx=e['handle'];vm=e['vm']
            if any(len(mapped(c['gpu_va'],c['size'],vm))!=1 or not uploaded(c,vm) for c in components.values()):
                raise ValueError('context dependencies not live at creation')
        elif op=='job' and e['type'] in (0,1) and e['context']==ctx:
            if any(len(mapped(c['gpu_va'],c['size'],vm))!=1 or not uploaded(c,vm) for c in components.values()):
                raise ValueError('context dependencies not live at job')
            jobs+=1
        elif op=='context_destroy' and e['handle']==ctx:ctx=None
    if not jobs:raise ValueError('no render jobs')
    return {'components':len(components),'render_jobs':jobs,'gpu_execution_allowed':False,
            'limitations':['GPU references embedded in context-switch programs are not decoded',
                           'upload padding, shader contents, CPU writes and later GPU accesses are unverified',
                           'shim observes requests without executing GPU work']}

def check(root):
    root=pathlib.Path(root);m=json.loads((root/'run-manifest.json').read_text());events=(root/'events.jsonl').read_bytes()
    if not (m.get('diagnostic_manifest') or {}).get('context_component_metadata_only') or hashlib.sha256(events).hexdigest()!=m['events_sha256']:
        raise ValueError('invalid capture provenance')
    spec=importlib.util.spec_from_file_location('trace',pathlib.Path(__file__).with_name('check-render-trace.py'))
    trace=importlib.util.module_from_spec(spec);spec.loader.exec_module(trace);trace.check(root/'events.jsonl')
    return analyze([json.loads(x) for x in events.splitlines()])
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try:print(json.dumps(check(a.capture),indent=2))
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):
        print('invalid or unsupported context capture',file=sys.stderr);return 2
    return 1
if __name__=='__main__':sys.exit(main())
