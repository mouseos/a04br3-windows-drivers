#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate an INCOMPLETE zero-only render fixture for offline analysis.

No BO data is copied; GPU execution is forbidden. The two geometry register
fields containing an address/encoded address are deliberately zeroed.
"""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import struct

GPU_LIMIT=1 << 40

def align(v,n): return (v+n-1)//n*n

def select(events, target):
    maps={}; bos={}; contexts={}; freelists={}; hwrts={}; submit=-1
    for e in events:
        op=e['event']
        if op=='bo_create': bos[(e['fd'],e['handle'])]=e['size']
        elif op=='bo_close': bos.pop((e['fd'],e['handle']),None)
        elif op=='context_create': contexts[e['handle']]=e
        elif op=='context_destroy': contexts.pop(e['handle'],None)
        elif op=='freelist_create': freelists[e['handle']]=e
        elif op=='freelist_destroy': freelists.pop(e['handle'],None)
        elif op=='hwrt_create': hwrts[e['handle']]=e
        elif op=='hwrt_destroy': hwrts.pop(e['handle'],None)
        elif op=='vm_map':
            key=(e['fd'],e['handle']); vm=e['vm']; va=e['va']; size=e['size']; off=e['offset']
            if key not in bos or size<=0 or va<=0 or va>=GPU_LIMIT or size>GPU_LIMIT-va or off<0 or off+size>bos[key]: raise ValueError('invalid map')
            if any(m['vm']==vm and va<m['va']+m['size'] and m['va']<va+size for m in maps.values()): raise ValueError('overlap')
            maps[(vm,va)]=dict(e)
        elif op=='vm_unmap':
            m=maps.pop((e['vm'],e['va']))
            if m['size']!=e['size']: raise ValueError('invalid unmap')
        elif op=='vm_destroy':
            maps={k:v for k,v in maps.items() if k[0]!=e['handle']}
        elif op=='submit_begin': submit+=1
        elif op=='job' and submit==target and e['type']==0:
            context=contexts[e['context']]; hwrt=hwrts[e['hwrt']]
            lists=[freelists[x] for x in hwrt['freelists']]
            return e, context, hwrt, lists, list(maps.values())
    raise ValueError('geometry job not found')

def make_plan(events, target):
    j,c,h,fl,maps=select(events,target)
    if (c['type']!=0 or (h['width'],h['height'],h['samples'],h['layers'])!=(64,64,1,1)
        or h['tpc_size']!=128 or h['tpc_stride']!=0 or h['region']!=16
        or h['rtc']!=0 or any(rt[1]!=0 for rt in h['rt'])
        or h['merge'] != [0]*6 or j['flags'] != 3 or j['rt_index'] != 0
        or sorted(f['max'] for f in fl) != [48,65536]
        or len(fl)!=2 or any(f['vm']!=c['vm'] or f['max']<=0 for f in fl)):
        raise ValueError('unsupported captured HWRT')
    stream=bytes.fromhex(j['stream_hex'])
    if (len(stream)!=48 or j['stream_length']!=48 or struct.unpack_from('<II',stream)!= (40,0)
        or struct.unpack_from('<IIIIII',stream,24)!=(514,131072,536870912,0,1,32)):
        raise ValueError('unsupported geometry stream')
    # Single-layer, GE8300 SIPF1 diagnostic branch: one 4x4 macrotile.
    # This branch has not been verified on real hardware.
    # free_list->size is the free-list table byte size in pvr_free_list.c.
    pages=(sum(f['max']*4 for f in fl))//4096
    div=lambda x,y:(x+y-1)//y
    pt=div(pages,512); pd=div(pt,512); pc=div(pd,1024)
    mlist=align((pt+pd+pc)*2*4,4096)
    if not mlist: raise ValueError('invalid MLIST size')
    # 4x4 tiles per one SIPF1 macrotile, 5-byte headers, 64-byte alignment.
    region=align(4*4*5,64)
    vheap=0x180*4
    roles=[('tpc',h['tpc'],128),('vheap',h['vheap'],vheap),
           ('mlist0',h['rt'][0][0],mlist),('mlist1',h['rt'][1][0],mlist),
           ('region0',h['rt'][0][2],region),('region1',h['rt'][1][2],region),
           ('freelist0',fl[0]['gpu'],fl[0]['max']*4),('freelist1',fl[1]['gpu'],fl[1]['max']*4)]
    ordered=sorted((addr,addr+length) for _,addr,length in roles)
    if any(end>nxt for (_,end),(nxt,_) in zip(ordered,ordered[1:])):
        raise ValueError('overlapping HWRT or freelist ranges')
    if h['rt'][1][0]-h['rt'][0][0]!=mlist or h['rt'][1][2]-h['rt'][0][2]!=region:
        raise ValueError('RT allocation stride mismatch')
    allocations={}; resolved=[]
    for name,addr,length in roles:
        matching=[m for m in maps if m['vm']==c['vm'] and addr>=m['va'] and length<=m['size'] and addr-m['va']<=m['size']-length]
        if len(matching)!=1: raise ValueError('required range unmapped or ambiguous')
        m=matching[0]; key=(m['va'],m['size'])
        if m['va']%4096 or m['size']%4096: raise ValueError('mapping not page aligned')
        allocations[key]=m
        resolved.append({'name':name,'gpu_va':addr,'length':length,'mapping_va':m['va'],'mapping_offset':addr-m['va']})
    vdm=struct.unpack_from('<Q',stream,8)[0]
    if not any(m['vm']==c['vm'] and m['va']<=vdm<m['va']+m['size'] for m in maps):
        raise ValueError('VDM start unmapped')
    sanitized=bytearray(stream); sanitized[8:24]=bytes(16)
    layouts=[]; file_offset=0
    for a,n in sorted(allocations):
        if n > 16*1024*1024-file_offset: raise ValueError('fixture too large')
        layouts.append({'gpu_va':a,'length':n,'file_offset':file_offset,'initialization':'zero'})
        file_offset += n
    return {'format':1,'zero_image_size':file_offset,'mesa_commit':'ef11349e8e5d620a2227fac0eed17e7ed1f86428',
      'bvnc':'22.67.54.30','submit':target,'gpu_execution_allowed':False,
      'geometry_template_hex':sanitized.hex(),'redacted_fields':['vdm_ctrl_stream_base','tpu_border_colour_table'],
      'hwrt':{'width':64,'height':64,'samples':1,'layers':1,'tpc_size':128,'tpc_stride':0,'region_header_size':16,'merge':h['merge']},
      'mappings':layouts,
      'ranges':resolved,
      'unresolved':['VDM and TPU address encodings','PDS/USC programs and indirect BO references','freelist physical PM pages and GPU writes','diagnostic SIPF1 allocation has not been confirmed on hardware','context static state and fragment command dependencies']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=pathlib.Path);p.add_argument('--submit',type=int,default=0);p.add_argument('--output',required=True,type=pathlib.Path);p.add_argument('--experimental-incomplete',action='store_true');a=p.parse_args()
    if not a.experimental_incomplete: raise SystemExit('refusing incomplete fixture; opt in to offline analysis only')
    manifest=json.loads((a.capture/'run-manifest.json').read_text())
    event=a.capture/'events.jsonl'; data=event.read_bytes()
    diag=manifest.get('diagnostic_manifest') or {}
    if not diag.get('sipf1_patch') or not diag.get('assertions_enabled_in_hwrt_unit'):
        raise SystemExit('corrected assert-enabled SIPF1 diagnostic capture required')
    if hashlib.sha256(data).hexdigest()!=manifest['events_sha256'] or manifest['mesa_build_manifest']['mesa_commit']!='ef11349e8e5d620a2227fac0eed17e7ed1f86428' or manifest.get('shim_bvnc')!='22.67.54.30': raise SystemExit('capture provenance mismatch')
    spec=importlib.util.spec_from_file_location('trace_check', pathlib.Path(__file__).with_name('check-render-trace.py'))
    verifier=importlib.util.module_from_spec(spec);spec.loader.exec_module(verifier)
    if verifier.check(event)['unresolved_address_checks']:
        raise SystemExit('capture has unresolved HWRT address checks')
    plan=make_plan([json.loads(x) for x in data.splitlines()],a.submit)
    a.output.mkdir(mode=0o700,parents=True,exist_ok=False)
    (a.output/'fixture-plan.json').write_text(json.dumps(plan,sort_keys=True,indent=2)+'\n')
    (a.output/'geometry-template.bin').write_bytes(bytes.fromhex(plan['geometry_template_hex']))
    with (a.output/'zero-initialized-mappings.bin').open('wb') as f:
        remaining=plan['zero_image_size']
        while remaining:
            chunk=min(remaining,65536); f.write(bytes(chunk)); remaining-=chunk
    # No BO snapshot is read or copied. This output cannot be submitted to a GPU.
    print('offline plan created; GPU execution prohibited')
if __name__=='__main__': main()
