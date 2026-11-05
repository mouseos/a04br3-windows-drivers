#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check the no-draw 64x64 Mesa clear VDM without exporting command buffers."""
import hashlib
import importlib.util
import json
import pathlib
import re
import struct
import sys

def fnv(data):
    h=14695981039346656037
    for b in data: h=((h^b)*1099511628211)&((1<<64)-1)
    return h

def check(path):
    root=pathlib.Path(path);manifest=json.loads((root/'run-manifest.json').read_text());diag=manifest.get('diagnostic_manifest') or {}
    if not manifest.get('scratchless_vertex') or not manifest.get('control_stream_dump') or not diag.get('vdm_hash_metadata_only'):
        raise ValueError('capture lacks provenance')
    log=(root/'probe.log').read_bytes();hashes=(root/'vdm-hashes.jsonl').read_bytes();events=(root/'events.jsonl').read_bytes()
    for raw,key in ((log,'probe_log_sha256'),(hashes,'vdm_hashes_sha256'),(events,'events_sha256')):
        if hashlib.sha256(raw).hexdigest()!=manifest.get(key): raise ValueError('capture digest mismatch')
    sections=log.decode('utf-8','replace').split('======= BEGIN CONTROL STREAM DUMP =======')
    if len(sections)<2: raise ValueError('missing clear VDM')
    sec=sections[1].split('======= END CONTROL STREAM DUMP =======',1)[0]
    if re.findall(r'^\[[0-9a-f]+\]\s+\d+:\s+([A-Z_]+)',sec,re.M)!=['PPP_STATE_UPDATE','TERMINATE']:
        raise ValueError('clear VDM shape mismatch')
    addrs=re.findall(r'^  addr\s*:\s*(0x[0-9a-f]+)',sec,re.M)
    if len(addrs)!=1: raise ValueError('clear PPP address missing')
    addr=int(addrs[0],16)
    if not addr or addr&3 or addr>=1<<40: raise ValueError('invalid PPP address')
    expected=struct.pack('<III',(3<<8)|(addr>>32),addr&0xffffffff,0xc0000000)
    rows=[json.loads(x) for x in hashes.splitlines()]
    if len(rows)!=3 or rows[0].get('type')!=1 or not rows[0].get('valid') or rows[0].get('bytes')!=12 or rows[0].get('fnv64')!=fnv(expected):
        raise ValueError('clear VDM digest mismatch')
    jobs=[];links=0
    for e in map(json.loads,events.splitlines()):
        if e['event']=='fragment_shader_link':links+=1
        if e['event']=='job' and e.get('type') in (0,1) and links==0: jobs.append(e)
    if len(jobs)!=3 or [(e['type'],e['flags']) for e in jobs]!=[(0,3),(1,192),(1,128)]: raise ValueError('clear jobs mismatch')
    g=bytes.fromhex(jobs[0]['stream_hex'])
    if len(g)!=48 or struct.unpack_from('<I',g,32)[0]!=0x20000000:raise ValueError('clear context resume mismatch')
    if jobs[1]['stream_hex']!=jobs[2]['stream_hex']:raise ValueError('partial/final mismatch')
    spec=importlib.util.spec_from_file_location('fixed',pathlib.Path(__file__).with_name('check-fixed-commands.py'))
    fixed=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixed)
    vdm,border=struct.unpack_from('<QQ',g,8);expected_g=bytearray(fixed.geometry(vdm,border<<2));struct.pack_into('<I',expected_g,32,0x20000000)
    if g!=bytes(expected_g):raise ValueError('clear geometry stream mismatch')
    f=bytes.fromhex(jobs[1]['stream_hex']);scissor=struct.unpack_from('<Q',f,8)[0]
    w0,w1,_=struct.unpack_from('<3Q',f,256);_,pw1,_=struct.unpack_from('<3Q',f,280)
    shader=fixed.PDS+(w0&0xfffffff0);code=fixed.PDS+((w0>>32)&0xfffffff0)
    data=fixed.PDS+((w1>>32)&0xfffffff0);prdata=fixed.PDS+((pw1>>32)&0xfffffff0)
    event=fixed.PDS+struct.unpack_from('<I',f,364)[0]
    if f!=fixed.fragment(scissor,border<<2,shader,code,data,prdata,event):raise ValueError('clear fragment stream mismatch')
    return {'clear_vdm_bytes':12,'geometry_jobs':1,'fragment_jobs':2,'gpu_execution_allowed':False,
            'limitations':['FNV is not collision resistant','capture does not execute GPU work','clear memory ownership and completion require separate validation']}

if __name__=='__main__':
    try:
        if len(sys.argv)!=2:raise ValueError('usage: check-clear-vdm.py PRIVATE_CAPTURE')
        print(json.dumps(check(sys.argv[1]),indent=2))
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError) as e:
        print('invalid capture: '+str(e),file=sys.stderr);sys.exit(2)
