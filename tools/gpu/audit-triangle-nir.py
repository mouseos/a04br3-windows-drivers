#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Audit the fixed shader NIR and draw bounds in a private Mesa capture."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import re
import sys

V_INTR={'load_scratch','load_vertex_id','store_output','store_scratch'}
F_INTR={'alpha_to_coverage','isp_feedback_pco','load_fs_meta_pco','load_sample_id','load_savmsk_vm_pco','pack_pco','store_output'}
V_CONST={0,3,8,16,0xbf400000,0x3f400000,0x3f000000,0x3f800000}
V_NOSCRATCH_INTR={'load_vertex_id','store_output'}
V_NOSCRATCH_CONST={0,1,2,0xbf400000,0x3f400000,0x3f000000,0x3f800000}
F_CONST={0,1,9,16,25,0x3f800000}
PROBE_COMMIT='5980bb779d837b9a00e03b799dc8119bee2cae4b'
PROBE_SOURCE_SHA256='e9da24b42baea04c55d8f30fd8d9c6574c078a6899840c5cc3fb0481b897f63b'

def audit(text,scratchless=False):
    sections=re.findall(r'after pco_postprocess_nir:\nshader: (MESA_SHADER_\w+)(.*?\n})',text,re.S)
    if [s for s,_ in sections]!=['MESA_SHADER_VERTEX','MESA_SHADER_FRAGMENT']*2:raise ValueError('unexpected NIR stages')
    for stage,body in sections:
        vertex=stage=='MESA_SHADER_VERTEX'
        intr=set(re.findall(r'@([a-z_]+)',body))
        if intr!=((V_NOSCRATCH_INTR if scratchless else V_INTR) if vertex else F_INTR):raise ValueError('unsupported intrinsic set')
        constants=[]
        for expr in re.findall(r'load_const \(([^)]*)\)',body):
            words=re.findall(r'0x([0-9a-fA-F]{8})',expr)
            if not words:raise ValueError('unsupported constant format')
            constants.extend(int(x,16) for x in words)
        if set(constants)!=((V_NOSCRATCH_CONST if scratchless else V_CONST) if vertex else F_CONST):raise ValueError('unexpected NIR constants')
        if vertex and scratchless:
            if re.search(r'^scratch:\s*[1-9]',body,re.M):raise ValueError('unexpected scratch allocation')
            vid=re.findall(r'%([0-9]+)\s*=\s*@load_vertex_id\b',body)
            const={k:int(v,16) for k,v in re.findall(r'%([0-9]+)\s*=\s*load_const\s*\(0x([0-9a-fA-F]{8})',body)}
            eq=re.findall(r'%([0-9]+)\s*=\s*ieq\s+%([0-9]+),\s*%([0-9]+)',body)
            select=re.findall(r'%([0-9]+)\s*=\s*bcsel\s+%([0-9]+),\s*%([0-9]+)[^,]*,\s*%([0-9]+)',body)
            outputs=re.findall(r'@store_output\s*\(%([0-9]+)[^\n]*?component=([0-3])[^\n]*?io location=VARYING_SLOT_POS',body)
            if len(vid)!=1 or len(eq)!=2 or len(select)!=2 or len(outputs)!=4:raise ValueError('unexpected vertex expression')
            eqmap={a:(b,const.get(c)) for a,b,c in eq}
            selmap={a:(b,const.get(c),const.get(d)) for a,b,c,d in select}
            outmap={int(component):value for value,component in outputs}
            if set(outmap)!={0,1,2,3} or const.get(outmap[2])!=0x3f000000 or const.get(outmap[3])!=0x3f800000:
                raise ValueError('unexpected vertex position')
            for component,index in ((0,1),(1,2)):
                cond,yes,no=selmap.get(outmap[component],(None,None,None))
                if eqmap.get(cond)!=(vid[0],index) or yes!=0x3f400000 or no!=0xbf400000:
                    raise ValueError('unexpected triangle vertices')
        if vertex and not scratchless:
            if not re.search(r'^scratch:\s*24$',body,re.M):raise ValueError('unexpected scratch size')
            ids=re.findall(r'%([0-9]+)\s*=\s*@load_vertex_id\b',body)
            threes=re.findall(r'%([0-9]+)\s*=\s*load_const \(0x00000003\)',body)
            if len(ids)!=1 or len(threes)!=1:raise ValueError('vertex index source')
            shifted=re.findall(r'%([0-9]+)\s*=\s*ishl\s+%'+ids[0]+r',\s*%'+threes[0]+r'\b',body)
            loads=re.findall(r'32x2\s+%[0-9]+\s*=\s*@load_scratch\s*\(%([0-9]+)\)',body)
            stores=re.findall(r'@store_scratch\s*\([^\n]*?,\s*%[0-9]+\s*\(0x([0-9a-fA-F]{1,8})[^)]*\)\)',body)
            if len(shifted)!=1 or loads!=shifted or sorted(int(x,16) for x in stores)!=[0,8,16]:raise ValueError('scratch access layout')
    blocks=re.findall(r'\[[0-9a-fA-F]+\]\s+\d+: INDEX_LIST\n(.*?)\n\s*<raw>',text,re.S)
    if len(blocks)!=2:raise ValueError('unexpected index list count')
    for b in blocks:
        fields=dict(re.findall(r'^\s*([a-z_]+)\s*:\s*([^\n]+)',b,re.M))
        if fields.get('index_count')!='3' or not fields.get('primitive_topology','').startswith('TRI_LIST (6)'):
            raise ValueError('unsupported draw')
        for key in ('index_base_addr','index_offset','start_index','start_instance','indirect_base_addr','instance_count'):
            if fields.get(key)!='<not present>':raise ValueError('unsupported indexed or instanced draw')
    if scratchless:
        grouped=re.findall(r'shader ir after pco_group_instrs:\nname:.*?\nstage: (MESA_SHADER_\w+)(.*?\n})',text,re.S)
        if [s for s,_ in grouped]!=['MESA_SHADER_VERTEX','MESA_SHADER_FRAGMENT']*2:raise ValueError('missing grouped PCO')
        allowed={'bbyp0bm_imm32','mbyp','movc','pck.zero','tst.s32.e','tst.u32.z.phase2end','uvsw.write','uvsw.write.emit.endtask'}
        for stage,body in grouped:
            if stage!='MESA_SHADER_VERTEX':continue
            phases=re.findall(r'(?:p\d(?:_[a-z]+)?|backend|ctrl):\s*([a-z][a-z0-9_.]*)',body)
            if set(phases)!=allowed or len(re.findall(r'^\s*\d+:',body,re.M))!=13:
                raise ValueError('unsupported grouped vertex instructions')
            immediates=re.findall(r'bbyp0bm_imm32[^\n;]*,\s*(-?\d+\.\d+f)',body)
            if sorted(immediates)!=['-0.750000f','-0.750000f','0.750000f','0.750000f']:
                raise ValueError('unexpected grouped immediates')
    return {'vertex_shaders':2,'fragment_shaders':2,'draws':2,'vertex_scratch_bytes':0 if scratchless else 24,
            'vertex_scratch_formula':'none in NIR' if scratchless else 'vertex_id * 8, 8-byte load',
            'gpu_execution_allowed':False,
            'limitations':['bounds depend on fixed non-indexed three-vertex draw and VDM behavior',
              ('NIR and grouped PCO have no scratch operation; machine-code decoding and runtime behavior remain unproven' if scratchless else
               'physical scratch address depends on local_addr_inst_num; its device bound is not measured'),
              'NIR is not machine-code decoding or proof of every USC immediate',
              'context switch shared-state access and other indirect GPU references remain unverified']}

def check(ir,ones):
    ir=pathlib.Path(ir);m=json.loads((ir/'run-manifest.json').read_text());raw=(ir/'probe.log').read_bytes()
    if (not m.get('private_pco_ir') or m.get('probe_commit')!=PROBE_COMMIT or
        m.get('probe_source_sha256')!=PROBE_SOURCE_SHA256 or hashlib.sha256(raw).hexdigest()!=m.get('probe_log_sha256')):
        raise ValueError('IR provenance')
    spec=importlib.util.spec_from_file_location('usc',pathlib.Path(__file__).with_name('check-triangle-usc-candidates.py'))
    checker=importlib.util.module_from_spec(spec);spec.loader.exec_module(checker);checker.compare(ir,ones)
    return audit(raw.decode('utf-8','replace'),bool(m.get('scratchless_vertex')))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('ir');p.add_argument('ones');a=p.parse_args()
    try:print(json.dumps(check(a.ir,a.ones),indent=2));return 1
    except (OSError,ValueError,KeyError,TypeError,json.JSONDecodeError):
        print('invalid or unsupported triangle NIR capture',file=sys.stderr);return 2
if __name__=='__main__':sys.exit(main())
