#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check two ordered render cases, their CRCs and shared shutdown in one boot."""
import argparse
import hashlib
import importlib.util
import json
import os
import pathlib
import re
import sys

HERE=pathlib.Path(__file__).resolve().parent

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);return mod

def tile_counts(data):
    kinds=(bytes.fromhex('224466ff'),bytes.fromhex('ff00ffff'),bytes.fromhex('a55ac37e'),bytes(4))
    result=[]
    for ty in range(4):
        row=[]
        for tx in range(4):
            counts=[0]*5
            for y in range(ty*16,ty*16+16):
                for x in range(tx*16,tx*16+16):
                    p=data[(y*64+x)*4:(y*64+x+1)*4]
                    counts[kinds.index(p) if p in kinds else 4]+=1
            row.append(dict(zip(('background','triangle','sentinel','zero','other'),counts)))
        result.append(row)
    return result

def analyze(data):
    one=load('one',HERE/'analyze-ge8300-fw-evidence.py');pixels=load('pixels',HERE.parent/'gpu/extract-render-pixels.py')
    marks=list(re.finditer(rb'A04GPU CASE (BEGIN|END)=([0-9A-F]{8})\r?\n',data))
    if [(m[1],int(m[2],16)) for m in marks]!=[(b'BEGIN',0),(b'END',0),(b'BEGIN',1),(b'END',1)]:raise ValueError('missing, duplicated or out-of-order cases')
    if data.count(b'A04GPU CASE BEGIN=')!=2 or data.count(b'A04GPU CASE END=')!=2 or b'A04GPU CASE SKIPPED=' in data or b'A04GPU CASE ABORT=' in data:raise ValueError('case incomplete')
    stops=list(one.SHUT.finditer(data))
    if len(stops)!=1 or stops[0].start()<marks[-1].end() or data.count(b'A04GPU RENDER SHUTDOWN STATUS=')!=1:raise ValueError('missing or ambiguous shutdown')
    prefix=data[:marks[0].start()];shutdown=data[stops[0].start():stops[0].end()];cases=[];sentinel_masks=[]
    for index,(start,end) in enumerate(((marks[0],marks[1]),(marks[2],marks[3]))):
        section=data[start.end():end.start()]
        report=one.analyze(prefix+section+shutdown)
        rgba=pixels.extract(section)
        sentinel_masks.append(tuple(rgba[i:i+4]==bytes.fromhex('a55ac37e') for i in range(0,len(rgba),4)))
        report['case']=('triangle' if index==0 else 'clear_only')
        report['tiles_16x16']=tile_counts(rgba)
        cases.append(report)
    clear=cases[1]['render'];tri=cases[0]['render']
    return {'cases':cases,'clear_full_background':clear['background']==4096 and clear['other']==0 and clear['triangle']==0,
            'triangle_reported_status_success':tri['status']==0,'same_sentinel_mask':sentinel_masks[0]==sentinel_masks[1],
            'limitations':['matching sentinel masks do not prove identical GPU writes or cache visibility','clear coverage and triangle coverage do not alone prove the source of a mismatch','FW snapshots are non-atomic','no result proves independent GPU DMA quiescence']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('uart',type=pathlib.Path);p.add_argument('--offset',type=int,default=0);p.add_argument('--end',type=int);p.add_argument('--output-dir',type=pathlib.Path)
    a=p.parse_args()
    if a.offset<0 or (a.end is not None and a.end<a.offset):raise ValueError('invalid range')
    with a.uart.open('rb') as f:
        f.seek(a.offset);raw=f.read() if a.end is None else f.read(a.end-a.offset)
    result=analyze(raw)
    if a.output_dir is not None:
        if not a.output_dir.is_dir():raise ValueError('output directory must already exist')
        marks=list(re.finditer(rb'A04GPU CASE (BEGIN|END)=([0-9A-F]{8})\r?\n',raw))
        pixels=load('pixels_output',HERE.parent/'gpu/extract-render-pixels.py')
        frames=[pixels.extract(raw[marks[i].end():marks[i+1].start()]) for i in (0,2)]
        paths=[a.output_dir/'triangle.rgba',a.output_dir/'clear.rgba'];created=[]
        try:
            for path,frame in zip(paths,frames):
                fd=os.open(path,os.O_WRONLY|os.O_CREAT|os.O_EXCL|getattr(os,'O_NOFOLLOW',0),0o600);created.append(path)
                with os.fdopen(fd,'wb') as f:f.write(frame)
        except (OSError,ValueError):
            for path in created:path.unlink()
            raise
    print(json.dumps(result,indent=2))
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,IndexError) as e:print(str(e),file=sys.stderr);sys.exit(1)
