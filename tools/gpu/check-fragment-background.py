#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Trace fixed render background and event PDS ranges at fragment submission."""
import argparse,hashlib,importlib.util,json,pathlib,struct,sys
HERE=pathlib.Path(__file__).resolve().parent
BASE=0xda00000000

def analyze(events,event_traced=False,pbe_traced=False):
    maps={};contexts={};uploads=[];pixel_events={};targets=[];links=0;geometry=0;checked=0;refs=0;eot_checked=0;pbe_checked=0
    def mapping(vm,addr,size):
        return [k for k,n in maps.items() if k[0]==vm and addr>=k[1] and size<=n and addr-k[1]<=n-size]
    def require(vm,offset,allocation,kind,written,relative=0):
        nonlocal refs
        addr=BASE+offset
        if len(mapping(vm,addr,allocation))!=1:raise ValueError('background allocation not mapped')
        found=[u for u in uploads if u[0][0]==vm and u[1]==addr+relative and u[2]==written and u[3]==kind]
        if len(found)!=1:raise ValueError('background upload not live')
        refs+=1
    for e in events:
        op=e['event']
        if op=='vm_map':maps[(e['vm'],e['va'])]=e['size']
        elif op=='vm_unmap':
            key=(e['vm'],e['va']);maps.pop(key)
            uploads=[u for u in uploads if u[0]!=key]
        elif op=='vm_destroy':
            maps={k:v for k,v in maps.items() if k[0]!=e['handle']};uploads=[u for u in uploads if u[0][0]!=e['handle']]
        elif op=='context_create':contexts[e['handle']]=e
        elif op=='context_destroy':contexts.pop(e['handle'])
        elif op in ('upload','pds_construct'):
            addr,size=e['gpu_va'],e['size'];owners=[k for k,n in maps.items() if addr>=k[1] and size<=n and addr-k[1]<=n-size]
            if len(owners)!=1:raise ValueError('ambiguous upload')
            k=owners[0];uploads=[u for u in uploads if u[0][0]!=k[0] or not(addr<u[1]+u[2] and u[1]<addr+size)]
            uploads.append((k,addr,size,e['kind']))
        elif op=='fragment_shader_link':links+=1
        elif op=='render_pixel_event':pixel_events[e['data_gpu']]=e
        elif op=='render_pbe_target':targets.append(e)
        elif op=='job' and e['type']==0 and geometry<links:geometry+=1
        elif op=='job' and e['type']==1 and geometry and geometry==links:
            context=contexts.get(e['context']);vm=context['vm'] if context else None
            raw=bytes.fromhex(e.get('stream_hex',''))
            if len(raw)!=384 or e['stream_length']!=384 or e['flags'] not in (128,192):raise ValueError('unsupported fragment stream')
            for offset in (256,280):
                word0,word1,info=struct.unpack_from('<3Q',raw,offset)
                shader=word0&0xfffffff0;texcode=(word0>>32)&0xfffffff0
                texdata=(word1>>32)&0xfffffff0
                if (word0&0xf or word1&0xffffffff or info!=0x0080000000010000 or not shader or not texcode or not texdata):
                    raise ValueError('unsupported background state')
                require(vm,shader,48,'pds_data',32)
                require(vm,shader,48,'pds_code',4,32)
                require(vm,texcode,16,'pds_code',8)
                require(vm,texdata,16,'pds_data',12)
            event_info=struct.unpack_from('<I',raw,356)[0]
            event_data=struct.unpack_from('<I',raw,364)[0]
            if event_info!=0x201 or event_data&15 or not event_data:raise ValueError('unsupported pixel event')
            require(vm,event_data,16,'pds_data',8)
            if event_traced:
                event=pixel_events.get(BASE+event_data)
                if not event or event['data_size']!=8 or event['shader_size']!=128 or event['shader_gpu']>>32!=0xe0 or event['shader_gpu']&3:
                    raise ValueError('pixel event shader metadata')
                control=event['control'];shader=event['shader_gpu']
                if (control&0xffffffff)!=(shader&0xfffffffc) or control&~0x3feffffffff or control&0x3fe00000000!=0x800000000:
                    raise ValueError('pixel event DOUTU mismatch')
                h=14695981039346656037
                for byte in struct.pack('<Q',control):h=((h^byte)*1099511628211)&((1<<64)-1)
                if h!=event['fnv64'] or len(mapping(vm,shader,128))!=1 or sum(u[0][0]==vm and u[1]==shader and u[2]==40 and u[3]=='usc_code' for u in uploads)!=1:
                    raise ValueError('pixel event shader not live')
                eot_checked+=1
                if pbe_traced:
                    matches=[t for t in targets if t['state0']==event['pbe_state0'] and t['state1']==event['pbe_state1']]
                    if not matches:raise ValueError('pixel event PBE target missing')
                    for t in matches:
                        addr=(t['state0']<<2)|((t['state1']&63)<<34)
                        if ((t['width'],t['height'],t['stride'],t['image_size'])!=(64,64,64,16384) or
                            addr!=t['target_gpu'] or addr!=t['image_gpu'] or t['layout'] not in (1,2) or
                            len(mapping(vm,t['image_gpu'],t['image_size']))!=1):
                            raise ValueError('unsupported PBE target extent')
                    if len({(t['image_gpu'],t['image_size'],t['layout']) for t in matches})!=1:
                        raise ValueError('ambiguous PBE target')
                    pbe_checked+=1
            checked+=1
    if checked!=4 or geometry!=2 or links!=2:raise ValueError('incomplete render jobs')
    return {'fragment_jobs':checked,'background_pds_references':refs,'pixel_event_shader_jobs':eot_checked,
      'pbe_target_jobs':pbe_checked,'gpu_execution_allowed':False,
      'limitations':['PDS ranges are checked at submission, not through GPU completion',
        'background PDS and USC indirect references and source bytes are not reconstructed',
        'pixel event USC and PBE output or partial-render scratch are not validated',
        'TPU border table extent and GPU cache behavior remain unverified']}

def check(root):
    root=pathlib.Path(root);m=json.loads((root/'run-manifest.json').read_text());diag=m.get('diagnostic_manifest') or {}
    raw=(root/'events.jsonl').read_bytes()
    if not m.get('scratchless_vertex') or not diag.get('scissor_metadata_only') or hashlib.sha256(raw).hexdigest()!=m.get('events_sha256'):
        raise ValueError('capture provenance')
    spec=importlib.util.spec_from_file_location('submit',HERE/'check-render-submit-dependencies.py');dep=importlib.util.module_from_spec(spec);spec.loader.exec_module(dep)
    dep.check(root)
    return analyze([json.loads(x) for x in raw.splitlines()],bool(diag.get('render_pixel_event_metadata_only')),
                   bool(diag.get('pbe_target_metadata_only')))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture');a=p.parse_args()
    try:print(json.dumps(check(a.capture),indent=2));return 1
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError,struct.error):
        print('invalid fragment background capture',file=sys.stderr);return 2
if __name__=='__main__':sys.exit(main())
