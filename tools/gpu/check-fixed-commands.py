#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reconstruct fixed render register streams from typed metadata, never raw BOs."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import struct
import sys
HERE=pathlib.Path(__file__).resolve().parent
PDS=0xda00000000

def geometry(vdm,border):
    b=bytearray(48);struct.pack_into('<IIQQIIIIII',b,0,40,0,vdm,border>>2,0x202,0x20000,0x20010000,0,1,0x20)
    return bytes(b)
def fragment(scissor,border,bg_shader,bg_code,bg_data,pr_data,event):
    b=bytearray(384);struct.pack_into('<I',b,0,376);struct.pack_into('<Q',b,8,scissor)
    struct.pack_into('<QQ',b,56,0x0200007c68800000,0x0000003f0000003f)
    struct.pack_into('<Q',b,248,border>>2)
    for off,data in ((256,bg_data),(280,pr_data)):
        struct.pack_into('<3Q',b,off,((bg_code&0xffffffff)<<32)|(bg_shader&0xffffffff),(data&0xffffffff)<<32,0x0080000000010000)
    for off,value in ((336,0x1ffc),(340,0x3f800000),(344,0x3ff),(352,0x33000),(356,0x201),
                      (364,event&0xffffffff),(376,2),(380,0x20)):
        struct.pack_into('<I',b,off,value)
    return bytes(b)

def check(root):
    root=pathlib.Path(root);m=json.loads((root/'run-manifest.json').read_text());raw=(root/'events.jsonl').read_bytes()
    if hashlib.sha256(raw).hexdigest()!=m.get('events_sha256'):raise ValueError('capture hash')
    spec=importlib.util.spec_from_file_location('bg',HERE/'check-fragment-background.py');bg=importlib.util.module_from_spec(spec);spec.loader.exec_module(bg)
    report=bg.check(root)
    if report['background_clear_dependencies']!=8 or report['background_shader_dependencies']!=8 or report['pbe_target_jobs']!=4:
        raise ValueError('background provenance')
    for name in ('check-fixed-vdm.py','check-fixed-ppp.py'):
        spec=importlib.util.spec_from_file_location('capturecheck',HERE/name)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);module.check(root)
    events=[json.loads(x) for x in raw.splitlines()];links=0;g=None;checked=0;maps={};contexts={};scissors=[]
    def mapped(vm,addr,size):
        return sum(k[0]==vm and addr>=k[1] and size<=n and addr-k[1]<=n-size for k,n in maps.items())==1
    for e in events:
        if e['event']=='vm_map':maps[(e['vm'],e['va'])]=e['size']
        elif e['event']=='vm_unmap':maps.pop((e['vm'],e['va']))
        elif e['event']=='vm_destroy':maps={k:v for k,v in maps.items() if k[0]!=e['handle']}
        elif e['event']=='context_create':contexts[e['handle']]=e['vm']
        elif e['event']=='context_destroy':contexts.pop(e['handle'])
        elif e['event']=='scissor_table':scissors.append(e)
        elif e['event']=='fragment_shader_link':links+=1
        elif e['event']=='job' and e['type']==0 and links==1:
            rawg=bytes.fromhex(e['stream_hex']);vdm,border=struct.unpack_from('<QQ',rawg,8);border<<=2
            vm=contexts[e['context']]
            if e['flags']!=3 or rawg!=geometry(vdm,border) or not mapped(vm,vdm,60) or not mapped(vm,border,262144):raise ValueError('geometry command')
            g=(vdm,border)
        elif e['event']=='job' and e['type']==1 and links==1 and g:
            b=bytes.fromhex(e['stream_hex']);scissor=struct.unpack_from('<Q',b,8)[0]
            w0,w1,_=struct.unpack_from('<3Q',b,256);_,pw1,_=struct.unpack_from('<3Q',b,280)
            shader=PDS+(w0&0xfffffff0);code=PDS+((w0>>32)&0xfffffff0)
            data=PDS+((w1>>32)&0xfffffff0);prdata=PDS+((pw1>>32)&0xfffffff0)
            event=PDS+struct.unpack_from('<I',b,364)[0]
            vm=contexts[e['context']]
            if (e['flags'] not in (128,192) or b!=fragment(scissor,g[1],shader,code,data,prdata,event) or
                not mapped(vm,scissor,8) or not mapped(vm,g[1],262144) or
                not any(x['gpu_va']==scissor and x['size']==8 for x in scissors)):
                raise ValueError('fragment command')
            checked+=1
    if checked!=2:raise ValueError('incomplete fixed draw')
    return {'geometry_jobs':1,'fragment_jobs':checked,'gpu_execution_allowed':False,
      'limitations':['PBE actual write bounds and machine semantics remain unverified','job completion lifetime requires owned DMA and firmware cleanup']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try:print(json.dumps(check(a.capture),indent=2))
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError,struct.error):
        print('invalid fixed command capture',file=sys.stderr);return 2
    return 1
if __name__=='__main__':sys.exit(main())
