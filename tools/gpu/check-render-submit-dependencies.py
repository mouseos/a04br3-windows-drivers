#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check fixed-probe observed dependencies at submit, without exporting BOs."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import re
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent

def load(name):
    spec = importlib.util.spec_from_file_location('render_'+name.replace('-', '_'), ROOT/(name+'.py'))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

def analyze(events, text, scratchless=False, fs_meta_traced=False, scissor_traced=False):
    def fields(name):
        values=[int(x,16) for x in re.findall(r'^\s*'+name+r'\s*:\s*(0x[0-9a-fA-F]+)',text,re.M)]
        if len(values)!=2: raise ValueError('unsupported control stream')
        return values
    vertex=fields('vs_pds_data_base_addr'); descriptor=fields('pds_data_addr')
    vertex_code=fields('vs_pds_code_base_addr'); descriptor_code=fields('pds_code_addr')
    fragment_data=fields('uniformdatabase') if fs_meta_traced else []
    fragment_code=fields('texunicodebase') if fs_meta_traced else []
    maps={}; uploads=[]; contexts={}; dependencies={}; shaders={}; fs_meta={}; scissors={}; links=[]; checked=0;frag_checked=0
    def mapped(vm,addr,size):
        return sum(k[0]==vm and addr>=k[1] and size<=n and addr-k[1]<=n-size for k,n in maps.items())==1
    def active(vm,addr,size,kind):
        return sum(u[0][0]==vm and u[1]==addr and u[2]==size and u[3]==kind for u in uploads)==1
    for e in events:
        op=e['event']
        if op=='vm_map': maps[(e['vm'],e['va'])]=e['size']
        elif op=='vm_unmap':
            key=(e['vm'],e['va']);maps.pop(key)
            uploads=[u for u in uploads if u[0]!=key]
        elif op=='vm_destroy':
            maps={k:v for k,v in maps.items() if k[0]!=e['handle']}
            uploads=[u for u in uploads if u[0][0]!=e['handle']]
        elif op=='context_create': contexts[e['handle']]=e
        elif op=='context_destroy': contexts.pop(e['handle'])
        elif op in ('upload','pds_construct'):
            a,n=e['gpu_va'],e['size']; matches=[k for k,v in maps.items() if a>=k[1] and n<=v and a-k[1]<=v-n]
            if len(matches)!=1: raise ValueError('ambiguous upload')
            key=matches[0]
            uploads=[u for u in uploads if u[0][0]!=key[0] or not (a<u[1]+u[2] and u[1]<a+n)]
            uploads.append((key,a,n,e['kind']))
        elif op=='pds_shader': shaders[e['parent_gpu']]=e
        elif op=='pds_dependency': dependencies[(e['parent_gpu'],e['kind'])]=e
        elif op=='pds_fs_meta': fs_meta[e['parent_gpu']]=e
        elif op=='scissor_table':scissors[e['gpu_va']]=e
        elif op=='fragment_shader_link': links.append(e)
        elif op=='job' and e['type']==0 and checked<len(links):
            if len(links)!=checked+1 or checked>=2: raise ValueError('ambiguous job order')
            c=contexts.get(e['context']); link=links[checked]
            if not c or c['type']!=0: raise ValueError('render context unavailable')
            vm=c['vm'];v=vertex[checked];d=descriptor[checked]
            desc_size=12 if scratchless else 24;desc_fetch=16 if scratchless else 32
            if not mapped(vm,v,16) or not mapped(vm,d,desc_fetch) or not active(vm,v,12,'vertex_attributes') or not active(vm,d,desc_size,'descriptor_mappings'):
                raise ValueError('PDS data not live')
            for addr in (vertex_code[checked],descriptor_code[checked]):
                if sum(u[0][0]==vm and u[1]==addr and u[3]=='pds_code' and u[2]>0 for u in uploads)!=1:
                    raise ValueError('PDS code not live')
            shader=shaders.get(v)
            alloc,code=(256,176) if scratchless else (384,312)
            if not shader or shader['size']!=alloc or shader['entry_gpu']!=shader['shader_gpu'] or not mapped(vm,shader['shader_gpu'],alloc) or not active(vm,shader['shader_gpu'],code,'usc_code'):
                raise ValueError('vertex USC not live')
            for kind,size,block in ((('spill',8192,4),) if scratchless else (('spill',8192,4),('scratch',49152,24))):
                dep=dependencies.get((d,kind))
                if not dep or dep['buffer_size']!=size or dep['block_size']!=block or not mapped(vm,dep['info_gpu'],12) or not mapped(vm,dep['buffer_gpu'],size):
                    raise ValueError('special buffer not live')
            if fs_meta_traced:
                u=fragment_data[checked];uc=fragment_code[checked];fm=fs_meta.get(u);spill=dependencies.get((u,'spill'))
                if (not mapped(vm,u,32) or not active(vm,u,24,'descriptor_mappings') or not mapped(vm,uc,16) or
                    not active(vm,uc,8,'pds_code') or not fm or fm['offset']!=2 or fm['value']!=0x01fffffe or
                    not mapped(vm,fm['gpu_va'],4) or not spill or spill['buffer_size']!=8192 or spill['block_size']!=4 or
                    not mapped(vm,spill['info_gpu'],12) or not mapped(vm,spill['buffer_gpu'],8192)):
                    raise ValueError('fragment uniform dependency not live')
            if link['shader_size']!=448 or link['entry_gpu']!=link['shader_gpu']:
                raise ValueError('unsupported fragment shader')
            for field,sizefield,written,kind in (('pds_data_gpu','pds_data_size','pds_data_written','pds_data'),('pds_code_gpu','pds_code_size','pds_code_written','pds_code'),('shader_gpu','shader_size',None,'usc_code')):
                a=link[field];size=link[sizefield]; n=link[written] if written else 360
                if not mapped(vm,a,size) or not active(vm,a,n,kind): raise ValueError('fragment dependency not live')
            checked+=1
        elif op=='job' and e['type']==1 and scissor_traced and checked and checked==len(links):
            c=contexts.get(e['context']);vm=c['vm'] if c else None
            raw=bytes.fromhex(e.get('stream_hex',''))
            if len(raw)<16 or e['stream_length']!=len(raw):raise ValueError('missing fragment command')
            addr=struct.unpack_from('<Q',raw,8)[0];table=scissors.get(addr)
            h=14695981039346656037
            for byte in struct.pack('<II',64,64):h=((h^byte)*1099511628211)&((1<<64)-1)
            if not table or table['size']!=8 or table['fnv64']!=h or not mapped(vm,addr,8):raise ValueError('scissor table not live')
            frag_checked+=1
    if checked!=2 or len(links)!=2 or (scissor_traced and frag_checked!=4): raise ValueError('missing fixed-probe jobs')
    return {'fixed_draws_checked':checked,'fragment_fs_meta_checked':checked if fs_meta_traced else 0,
        'fragment_scissor_jobs_checked':frag_checked,'gpu_execution_allowed':False,
        'limitations':['recorded mapping and upload events do not cover every CPU write',
        'PDS code execution extent and all VDM/PPP references are not proven',
        'checks apply at geometry submission only, not until GPU completion',
        'shader immediate values and dynamic GPU accesses are not proven']}

def check(root):
    root=pathlib.Path(root);m=json.loads((root/'run-manifest.json').read_text())
    events=(root/'events.jsonl').read_bytes();log=(root/'probe.log').read_bytes()
    if hashlib.sha256(events).hexdigest()!=m['events_sha256'] or hashlib.sha256(log).hexdigest()!=m['probe_log_sha256']:
        raise ValueError('capture hash mismatch')
    load('check-render-vdm-pds').check(root)
    load('check-render-fragment').check(root)
    return analyze([json.loads(x) for x in events.splitlines()],log.decode('utf-8','replace'),bool(m.get('scratchless_vertex')),
                   bool((m.get('diagnostic_manifest') or {}).get('pds_fs_meta_metadata_only')),
                   bool((m.get('diagnostic_manifest') or {}).get('scissor_metadata_only')))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try: print(json.dumps(check(a.capture),indent=2));return 1
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):
        print('invalid or unsupported submit dependencies',file=sys.stderr);return 2
if __name__=='__main__':sys.exit(main())
