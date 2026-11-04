#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Rebuild two observed PDS constant maps; not a runnable GPU fixture."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import re
import struct
PIN='ef11349e8e5d620a2227fac0eed17e7ed1f86428'
MASK=(1<<64)-1

def fnv(data):
    h=14695981039346656037
    for b in data:h=((h^b)*1099511628211)&MASK
    return h

def build(events, text, index=0):
    if index not in (0,1):raise ValueError('unsupported index')
    keys=('vs_pds_data_base_addr','pds_data_addr')
    addresses=[]
    for key in keys:
        vals=[int(x,16) for x in re.findall(r'^\s*'+key+r'\s*:\s*(0x[0-9a-fA-F]+)',text,re.M)]
        if len(vals)!=2:raise ValueError('unexpected control stream references')
        addresses.append(vals[index])
    def construct(addr,kind,size,expected):
        cs=[e for e in events if e.get('event')=='pds_construct' and e['gpu_va']==addr and e['kind']==kind]
        entries=[e for e in events if e.get('event')=='pds_entry' and e['gpu_va']==addr and e['kind']==kind]
        if len(cs)!=1 or cs[0]['size']!=size or cs[0]['entries']!=len(entries):raise ValueError('construction metadata mismatch')
        if sorted((e['entry_type'],e['const_offset'],e['subtype']) for e in entries)!=sorted(expected):raise ValueError('unsupported constant map')
        out=bytearray(size);covered=set();deps={}
        for entry in entries:
            off=entry['const_offset']*4;t=entry['entry_type'];value=entry['value']
            if t==2:
                if not 0<=value<=0xffffffff:raise ValueError('literal outside 32 bits')
                raw=struct.pack('<I',value)
            elif t==6:
                matches=[e for e in events if e.get('event')=='pds_shader' and e['parent_gpu']==addr and e['offset']==entry['const_offset']]
                if len(matches)!=1 or value & ~0x3fe00000000:raise ValueError('unsupported shader control')
                sh=matches[0];gpu=sh['entry_gpu']
                if gpu>>32!=0xe0 or gpu&3 or not sh['shader_gpu']<=gpu<sh['shader_gpu']+sh['size']:raise ValueError('invalid USC entry')
                raw=struct.pack('<Q',(gpu&0xfffffffc)|value)
            elif t==5:
                matches=[e for e in events if e.get('event')=='pds_dependency' and e['parent_gpu']==addr and e['offset']==entry['const_offset']]
                if len(matches)!=1:raise ValueError('missing special buffer')
                dep=matches[0];sub=entry['subtype']
                if (sub,dep['kind'],dep['buffer_size'],dep['block_size']) not in ((12,'spill',8192,4),(13,'scratch',49152,24)):raise ValueError('unsupported buffer geometry')
                if not 0<dep['info_gpu']<1<<40 or not 0<dep['buffer_gpu']<1<<40 or dep['buffer_size']>(1<<40)-dep['buffer_gpu']:raise ValueError('invalid GPU pointer')
                raw=struct.pack('<Q',dep['info_gpu']);deps[dep['kind']]=struct.pack('<QI',dep['buffer_gpu'],dep['block_size'])
            else:raise ValueError('unsupported map entry')
            if off<0 or off+len(raw)>size or set(range(off,off+len(raw)))&covered:raise ValueError('entry overlap or bounds')
            covered.update(range(off,off+len(raw)));out[off:off+len(raw)]=raw
        if len(covered)!=size or fnv(out)!=cs[0]['fnv64']:raise ValueError('typed reconstruction mismatch')
        return bytes(out),deps
    vertex,_=construct(addresses[0],'vertex_attributes',12,[(2,2,0),(6,0,0)])
    descriptor,deps=construct(addresses[1],'descriptor_mappings',24,[(5,0,12),(5,2,13),(2,4,0),(2,5,0)])
    if set(deps)!= {'spill','scratch'}:raise ValueError('incomplete special buffers')
    # VDM fetches aligned ranges larger than the typed construction. Never
    # copy the unobserved padding from a BO or a host allocation.
    return {'vertex-pds.bin':vertex,'descriptor-pds.bin':descriptor,
            'vertex-pds-fetch.bin':vertex+bytes(4),'descriptor-pds-fetch.bin':descriptor+bytes(8),
            'spill-info.bin':deps['spill'],'scratch-info.bin':deps['scratch']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=pathlib.Path);p.add_argument('--output',required=True,type=pathlib.Path);p.add_argument('--index',type=int,default=0);p.add_argument('--experimental-incomplete',action='store_true');a=p.parse_args()
    if not a.experimental_incomplete:raise SystemExit('incomplete fixture: explicit offline opt-in required')
    manifest=json.loads((a.capture/'run-manifest.json').read_text());diag=manifest.get('diagnostic_manifest') or {}
    events=(a.capture/'events.jsonl').read_bytes();log=(a.capture/'probe.log').read_bytes()
    if (manifest['mesa_build_manifest']['mesa_commit']!=PIN or not diag.get('pds_construct_metadata_only') or
        not diag.get('sipf1_patch') or not diag.get('assertions_enabled_in_hwrt_unit') or not manifest.get('control_stream_dump') or
        hashlib.sha256(events).hexdigest()!=manifest['events_sha256'] or hashlib.sha256(log).hexdigest()!=manifest['probe_log_sha256']):raise SystemExit('capture provenance mismatch')
    spec=importlib.util.spec_from_file_location('trace',pathlib.Path(__file__).with_name('check-render-trace.py'));checker=importlib.util.module_from_spec(spec);spec.loader.exec_module(checker)
    result=checker.check(a.capture/'events.jsonl')
    if result['pds_dependency_unmapped'] or result['pds_shader_unmapped'] or result['uploads_ambiguous_vm'] or result['uploads_unmapped_at_observation']:raise SystemExit('GPU mappings incomplete')
    spec=importlib.util.spec_from_file_location('submit',pathlib.Path(__file__).with_name('check-render-submit-dependencies.py'))
    submit=importlib.util.module_from_spec(spec);spec.loader.exec_module(submit);submit.check(a.capture)
    files=build([json.loads(x) for x in events.splitlines()],log.decode('utf-8','replace'),a.index)
    a.output.mkdir(mode=0o700,parents=True,exist_ok=False)
    for name,data in files.items():(a.output/name).write_bytes(data)
    (a.output/'manifest.json').write_text(json.dumps({'gpu_execution_allowed':False,'capture_index':a.index,'files':{k:hashlib.sha256(v).hexdigest() for k,v in files.items()},'limitations':['no shader code','no fragment command stream','FNV is not a cryptographic proof','no hardware validation']},indent=2)+'\n')
    print('typed PDS data regenerated; GPU execution prohibited')
if __name__=='__main__':main()
