#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate default-off fixed triangle USC from independently checked PCO output."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import re

HERE=pathlib.Path(__file__).resolve().parent
EXPECTED=(('0-0.bin','a04_triangle_vertex_usc',176,'6e794f9ef7d983036038888b2db05a2959c349d2aed4e0f43cb7514357e732e4'),
          ('4-1.bin','a04_triangle_fragment_usc',360,'b44506355d85d2966805b1cc0b5b03a1ff16abf09d19e5dc07c1994c02c1ead5'))
FRAGMENT_OPS={'alphaf','bbyp0bm','bbyp0s1','logical.and','logical.or','mbyp','movc','msk_bbyp0s1',
    'pck.cov','pck.scale.u8888','pck.zero','savmsk.vm','shift.lsl','shift.shr','tst.s32.e','tst.s32.ne',
    'tst.u32.l.phase2end','tst.u32.z','tst.u32.z.phase2end','wdf'}

def check_fragment(log):
    grouped=re.findall(r'shader ir after pco_group_instrs:\nname:.*?\nstage: (MESA_SHADER_\w+)(.*?\n})',log,re.S)
    if [stage for stage,_ in grouped]!=['MESA_SHADER_VERTEX','MESA_SHADER_FRAGMENT']*2:raise ValueError('grouped stages')
    for stage,body in grouped:
        if stage!='MESA_SHADER_FRAGMENT':continue
        phases=set(re.findall(r'(?:p\d(?:_[a-z]+)?|backend|ctrl):\s*([a-z][a-z0-9_.]*)',body))
        if (phases!=FRAGMENT_OPS or len(re.findall(r'^\s*\d+:',body,re.M))!=27 or
            set(re.findall(r'\bsh\d+\b',body))!={'sh0'} or set(re.findall(r'\bsr\d+\b',body))!={'sr53'} or
            re.search(r'\b(?:imm|addr)\w*\b',body)):
            raise ValueError('unexpected fragment grouped PCO')

def generate(zero,ones):
    zero=pathlib.Path(zero);ones=pathlib.Path(ones)
    spec=importlib.util.spec_from_file_location('audit',HERE/'audit-triangle-nir.py')
    audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
    audit.check(zero,ones)
    for root,fill in ((zero,'00'),(ones,'ff')):
        m=json.loads((root/'run-manifest.json').read_text())
        if not m.get('scratchless_vertex') or m.get('pco_fill')!=fill:raise ValueError('capture variant')
        check_fragment((root/'probe.log').read_text())
    lines=['/* SPDX-License-Identifier: MIT',
           ' * Generated from pinned Mesa PCO using tools/gpu/generate-triangle-fixture.py.',
           ' * Diagnostic only; GPU machine semantics and hardware remain unverified. */',
           '#ifndef A04_RENDER_TRIANGLE_USC_H','#define A04_RENDER_TRIANGLE_USC_H','#include <stdint.h>']
    for filename,symbol,size,digest in EXPECTED:
        data=(zero/'private-compiler-candidates'/filename).read_bytes()
        other=(ones/'private-compiler-candidates'/filename).read_bytes()
        if len(data)!=size or data!=other or hashlib.sha256(data).hexdigest()!=digest:raise ValueError('shader mismatch')
        lines.append('static const uint8_t %s[%d] = {'%(symbol,size))
        for i in range(0,size,12):lines.append('    '+','.join('0x%02x'%x for x in data[i:i+12])+',')
        lines.append('};')
    lines.extend(['#endif','']);return '\n'.join(lines)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('zero','ones','output'):p.add_argument('--'+name,required=True,type=pathlib.Path)
    p.add_argument('--check',action='store_true');a=p.parse_args()
    try:
        value=generate(a.zero,a.ones)
        if a.check:
            if a.output.read_text()!=value:raise ValueError('header mismatch')
        else:
            with a.output.open('x') as f:f.write(value)
    except (OSError,ValueError,KeyError,TypeError):raise SystemExit('triangle fixture generation failed')
    print('triangle USC header verified; GPU execution remains prohibited')
if __name__=='__main__':main()
