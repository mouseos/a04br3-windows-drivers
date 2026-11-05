#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check layout and register encoding for the captured render target."""
import argparse,json,pathlib,struct,subprocess,xml.etree.ElementTree as ET
p=argparse.ArgumentParser();p.add_argument('--mesa',type=pathlib.Path,required=True);p.add_argument('--capture',type=pathlib.Path,action='append',required=True);a=p.parse_args()
if subprocess.check_output(['git','-C',str(a.mesa),'rev-parse','HEAD'],text=True).strip()!='ef11349e8e5d620a2227fac0eed17e7ed1f86428':raise SystemExit('Mesa revision mismatch')
s=(a.mesa/'src/imagination/vulkan/pvr_common.h').read_text()
if 'PVR_MEMLAYOUT_UNDEFINED = 0' not in s or s.index('PVR_MEMLAYOUT_LINEAR')>s.index('PVR_MEMLAYOUT_TWIDDLED'):raise SystemExit('unexpected enum')
x=ET.parse(a.mesa/'src/imagination/csbgen/rogue/pbestate.xml').getroot();layout=next(e for e in x.iter('enum') if e.get('name')=='MEMLAYOUT');values={v.get('name'):v.get('value') for v in layout.findall('value')}
if values.get('LINEAR')!='0':raise SystemExit('unexpected hardware layout')
for cap in a.capture:
 events=[json.loads(line) for line in (cap/'events.jsonl').read_text().splitlines()]
 targets=[e for e in events if e.get('event')=='render_pbe_target' and e.get('layout')==1 and e.get('width')==64 and e.get('height')==64 and e.get('stride')==64]
 if not targets:raise SystemExit('linear target metadata missing')
 jobs=[e for e in events if e.get('event')=='job' and e.get('type')==1 and e.get('stream_length')==384]
 words=[struct.unpack_from('<Q',bytes.fromhex(e['stream_hex']),56)[0] for e in jobs]
 if 0x0200007c68800000 not in words:raise SystemExit('PBE register not observed')
w=0x0200007c68800000
if ((w>>32)&3)!=0 or ((w>>34)&0xffff)!=31:raise SystemExit('PBE register is not linear 64-pixel stride')
print(json.dumps({'captures':len(a.capture),'mesa_layout':1,'hardware_memlayout':0,'hardware_linestride_field':31,'pixel_stride':64,'render_success_proven':False},indent=2))
