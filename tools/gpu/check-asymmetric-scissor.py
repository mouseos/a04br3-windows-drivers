#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Verify a scissor derived from captured metadata and pinned Mesa packing."""
import argparse, json, pathlib, struct, subprocess, xml.etree.ElementTree as ET
p=argparse.ArgumentParser();p.add_argument('--mesa',type=pathlib.Path,required=True);p.add_argument('--capture',type=pathlib.Path,action='append',required=True);a=p.parse_args()
if subprocess.check_output(['git','-C',str(a.mesa),'rev-parse','HEAD'],text=True).strip()!='ef11349e8e5d620a2227fac0eed17e7ed1f86428':raise SystemExit('Mesa commit mismatch')
x=ET.parse(a.mesa/'src/imagination/csbgen/rogue/ipf.xml').getroot()
for structname,names in [('SCISSOR_WORD_0',('scw0_xmin','scw0_xmax')),('SCISSOR_WORD_1',('scw1_ymin','scw1_ymax'))]:
 s=next(q for q in x.iter('struct') if q.get('name')==structname); fields={f.get('name'):(f.get('start'),f.get('end')) for f in s.findall('field')}
 if fields[names[0]]!=('16','31') or fields[names[1]]!=('0','15'):raise SystemExit('unexpected Mesa scissor packing')
def fnv(b):
 h=14695981039346656037
 for v in b:h=((h^v)*1099511628211)&((1<<64)-1)
 return h
for path in a.capture:
 rows=[json.loads(line) for line in (path/'events.jsonl').read_text().splitlines()]
 scissors=[r for r in rows if r.get('event')=='scissor_table']
 if not scissors or any(r['size']!=8 or r['fnv64']!=fnv(struct.pack('<II',64,64)) for r in scissors):raise SystemExit('capture scissor mismatch')
for align in (16,32):
 if (64+align-1)//align!=(49+align-1)//align:raise SystemExit('region clip changed')
print(json.dumps({'mesa_commit':'ef11349e8e5d620a2227fac0eed17e7ed1f86428','capture_count':len(a.capture),'original_words':[64,64],'derived_words':[49,64],'region_clip_unchanged_for_alignment':[16,32],'note':'derived from captured scissor and Mesa packing; no asymmetric recapture or GPU execution'},indent=2))
