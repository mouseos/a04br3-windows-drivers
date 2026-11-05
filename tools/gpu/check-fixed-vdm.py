#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Correlate a reconstructed fixed VDM stream with Mesa hashes, without raw BOs."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import re
import struct
import sys

HERE=pathlib.Path(__file__).resolve().parent

def fnv(data):
    h=14695981039346656037
    for b in data:h=((h^b)*1099511628211)&((1<<64)-1)
    return h

def streams(text,hashes):
    if len(hashes)!=3 or any(x.get('type')!=1 or not x.get('valid') for x in hashes) or [x['bytes'] for x in hashes]!=[12,60,60]:
        raise ValueError('unexpected stream hashes')
    sections=[x for x in re.split('CONTROL STREAM DUMP',text)[1:] if re.search(r'^\[[0-9a-f]+\]\s+\d+:\s+INDEX_LIST',x,re.M)]
    if len(sections)!=2:raise ValueError('unexpected draw dumps')
    out=[]
    for idx,sec in enumerate(sections):
        types=re.findall(r'^\[[0-9a-f]+\]\s+\d+:\s+([A-Z_]+)',sec,re.M)
        if types!=['PDS_STATE_UPDATE','PPP_STATE_UPDATE','VDM_STATE_UPDATE','INDEX_LIST','PPP_STATE_UPDATE','TERMINATE']:
            raise ValueError('unexpected VDM blocks')
        def field(name,count):
            v=[int(x,16) for x in re.findall(r'^  '+name+r'\s*:\s*(0x[0-9a-fA-F]+)',sec,re.M)]
            if len(v)!=count:raise ValueError('missing address field')
            return v
        d=field('pds_data_addr',1)[0];c=field('pds_code_addr',1)[0]
        v=field('vs_pds_data_base_addr',1)[0];vc=field('vs_pds_code_base_addr',1)[0]
        pp,finish=field('addr',2);a=(d,c,v,vc,pp,finish);length=(16,16,16,16,100,12)
        for i,(addr,size) in enumerate(zip(a,length)):
            if addr<=0 or addr>=(1<<40) or size>(1<<40)-addr or addr%(16 if i<4 else 4) or (i<4 and addr>>32!=0xda):
                raise ValueError('invalid VDM address')
            if any(addr<a[j]+length[j] and a[j]<addr+size for j in range(i)):raise ValueError('overlapping VDM input')
        words=(0x20010001,d&0xffffffff,c&0xffffffff,(25<<8)|(pp>>32),pp&0xffffffff,
          0x4c000f80,v&0xffffffff,vc&0xffffffff,5,0x20000441,0x68000600,3,
          (3<<8)|(finish>>32),finish&0xffffffff,0xc0000000)
        if fnv(struct.pack('<15I',*words))!=hashes[idx+1]['fnv64']:raise ValueError('VDM hash mismatch')
        out.append((pp,finish))
    return out

def ppp_mapped_at_job(events,ppp):
    maps={};contexts={};links=0;checked=0
    for e in events:
        op=e['event']
        if op=='vm_map':maps[(e['vm'],e['va'])]=e['size']
        elif op=='vm_unmap':maps.pop((e['vm'],e['va']))
        elif op=='vm_destroy':maps={k:v for k,v in maps.items() if k[0]!=e['handle']}
        elif op=='context_create':contexts[e['handle']]=e
        elif op=='context_destroy':contexts.pop(e['handle'])
        elif op=='fragment_shader_link':links+=1
        elif op=='job' and e['type']==0 and checked<links:
            if links!=checked+1 or checked>=len(ppp):raise ValueError('ambiguous job order')
            context=contexts.get(e['context']);vm=context['vm'] if context else None
            for addr,size in zip(ppp[checked],(100,12)):
                if sum(k[0]==vm and addr>=k[1] and size<=v and addr-k[1]<=v-size for k,v in maps.items())!=1:
                    raise ValueError('PPP not mapped at geometry job')
            checked+=1
    if checked!=2:raise ValueError('missing geometry jobs')

def check(root):
    root=pathlib.Path(root);m=json.loads((root/'run-manifest.json').read_text());diag=m.get('diagnostic_manifest') or {}
    if not m.get('scratchless_vertex') or not diag.get('vdm_hash_metadata_only') or not m.get('control_stream_dump'):
        raise ValueError('capture instrumentation missing')
    raw=(root/'probe.log').read_bytes();events=(root/'events.jsonl').read_bytes();hashes=(root/'vdm-hashes.jsonl').read_bytes()
    for data,key in ((raw,'probe_log_sha256'),(events,'events_sha256'),(hashes,'vdm_hashes_sha256')):
        if hashlib.sha256(data).hexdigest()!=m.get(key):raise ValueError('capture hash mismatch')
    spec=importlib.util.spec_from_file_location('submit',HERE/'check-render-submit-dependencies.py');submit=importlib.util.module_from_spec(spec);spec.loader.exec_module(submit)
    submit.check(root)
    ppp=streams(raw.decode('utf-8','replace'),[json.loads(x) for x in hashes.splitlines()])
    ppp_mapped_at_job([json.loads(x) for x in events.splitlines()],ppp)
    return {'draws':2,'vdm_bytes':60,'ppp_extents':[100,12],'gpu_execution_allowed':False,
      'limitations':['FNV comparison is not collision resistant','PPP bytes and indirect dependencies are not validated',
        'mapping at geometry submit does not prove GPU completion lifetime','capture does not execute GPU work']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try:print(json.dumps(check(a.capture),indent=2));return 1
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):print('invalid or unsupported VDM capture',file=sys.stderr);return 2
if __name__=='__main__':sys.exit(main())
