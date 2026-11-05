#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate one UART capture and summarize FW and tile evidence without raw pixels."""
import argparse
import hashlib
import importlib.util
import json
import pathlib
import re
import sys

HEX = rb'([0-9A-F]{8})'
FW = re.compile(rb'A04GPU FW_EVIDENCE PHASE='+HEX+rb' HWR='+HEX+rb' GEOM_LOCK='+HEX+rb' FRAG_LOCK='+HEX+rb' GEOM_RECOVER='+HEX+rb' FRAG_RECOVER='+HEX+rb' R='+HEX+rb' W='+HEX+rb' W_AFTER='+HEX+rb' RESET='+HEX+rb' GROW='+HEX+rb' REBUILD='+HEX+rb' RESTART='+HEX+rb' FW_PF='+HEX+rb' OTHER='+HEX+rb'\r?\n')
TILES = re.compile(rb'A04GPU TILE_SLOTS VALID='+HEX+rb' REGION_NONZERO='+HEX+rb' TPC_NONZERO='+HEX+rb'\r?\n')
LIFE = re.compile(rb'A04GPU RENDER LIFETIME COMPLETED='+HEX+rb' CLEANUP='+HEX+rb' PARTIAL='+HEX+rb' SUBMITTED='+HEX+rb' POISONED='+HEX+rb'\r?\n')
FENCE = re.compile(rb'A04GPU RENDER FENCES VALID='+HEX+rb' GEOM='+HEX+rb' FRAG='+HEX+rb' FRAG_EXPECT='+HEX+rb' GEOM_READ='+HEX+rb' GEOM_END='+HEX+rb' FRAG_READ='+HEX+rb' FRAG_END='+HEX+rb'\r?\n')
SHUT = re.compile(rb'A04GPU RENDER SHUTDOWN STATUS='+HEX+rb' RELEASED='+HEX+rb' STARTED='+HEX+rb' QUEUE_FAILED='+HEX+rb'\r?\n')
SENT = re.compile(rb'A04GPU RENDER SENTINEL='+HEX+rb'\r?\n')
RENDER = re.compile(rb'A04GPU RENDER STATUS='+HEX+rb' STAGE='+HEX+rb' BG='+HEX+rb' TRI='+HEX+rb' OTHER='+HEX+rb' ZERO='+HEX+rb' MISMATCH='+HEX+rb' BOUNDED='+HEX+rb' B0_VALID='+HEX+rb' B0='+HEX+rb' B1_VALID='+HEX+rb' B1='+HEX+rb'\r?\n')

def values(match,names): return dict(zip(names,(int(v,16) for v in match.groups())))
def exactly(data,pattern,label):
    found=list(pattern.finditer(data))
    if len(found)!=1: raise ValueError('missing or repeated '+label)
    return found[0]

def analyze(data):
    if data.count(b'A04BR3 recovery: EBS')!=1 or re.search(rb'A04WIN BUGCHECK|A04GPU BUGCHECK (?:(?:[0-9A-F]{8} ?){5}|DATA_UNAVAILABLE)\r?\n',data):
        raise ValueError('boot ambiguity or bugcheck')
    spec=importlib.util.spec_from_file_location('pixels',pathlib.Path(__file__).resolve().parents[1]/'gpu/extract-render-pixels.py')
    pixels=importlib.util.module_from_spec(spec);spec.loader.exec_module(pixels)
    rgba=pixels.extract(data)
    fw=list(FW.finditer(data))
    if len(fw)!=2 or [int(x[1],16) for x in fw]!=[0,1] or data.count(b'A04GPU FW_EVIDENCE PHASE=')!=2:
        raise ValueError('firmware snapshots missing, errored or repeated')
    tile=exactly(data,TILES,'tile evidence');life=exactly(data,LIFE,'lifetime');fence=exactly(data,FENCE,'fences');shutdown=exactly(data,SHUT,'shutdown');sent=exactly(data,SENT,'sentinel');render=exactly(data,RENDER,'render status')
    if not fw[0].start()<fw[1].start()<life.start()<fence.start()<tile.start()<shutdown.start(): raise ValueError('evidence ordering')
    names=('phase','hwr','geom_locked','frag_locked','geom_recovered','frag_recovered','read','write','write_after','reset','grow','reconstruct','restart','fw_pagefault','other')
    snapshots=[values(x,names) for x in fw]
    tile_values=values(tile,('valid','region_nonzero_slots','tpc_nonzero_slots'))
    life_values=values(life,('completed','cleanup','partial','submitted','poisoned'))
    fence_values=values(fence,('valid','geom','frag','frag_expected','geom_read','geom_end','frag_read','frag_end'))
    stop=values(shutdown,('status','released','started','queue_failed'))
    render_values=values(render,('status','stage','background','triangle','other','zero','mismatch','bounded','b0_valid','b0','b1_valid','b1'))
    sentinel=int(sent[1],16); count=sum(rgba[i:i+4]==bytes.fromhex('a55ac37e') for i in range(0,len(rgba),4))
    if count!=sentinel: raise ValueError('sentinel count differs from pixels')
    bg=bytes.fromhex('224466ff');tri=bytes.fromhex('ff00ffff');zero=bytes(4)
    colors=[rgba[i:i+4] for i in range(0,len(rgba),4)]
    if (render_values['stage']!=32 or render_values['background']!=colors.count(bg) or
        render_values['triangle']!=colors.count(tri) or render_values['zero']!=colors.count(zero) or
        render_values['other']!=4096-colors.count(bg)-colors.count(tri)):
        raise ValueError('render summary differs from pixels')
    if tile_values['valid']!=1 or tile_values['region_nonzero_slots']>0xffff or tile_values['tpc_nonzero_slots']>0xffff: raise ValueError('tile evidence invalid')
    if life_values!={'completed':1,'cleanup':1,'partial':1,'submitted':1,'poisoned':0}: raise ValueError('render lifecycle incomplete')
    if fence_values['valid']!=1 or fence_values['geom']!=1 or fence_values['frag']!=2 or fence_values['frag_expected']!=2 or fence_values['geom_read']!=fence_values['geom_end'] or fence_values['frag_read']!=fence_values['frag_end']: raise ValueError('fences incomplete')
    if stop!={'status':0,'released':0,'started':0,'queue_failed':0}: raise ValueError('shutdown incomplete')
    if any(s['read']>=32 or s['write']>=32 or s['write_after']>=32 for s in snapshots):raise ValueError('invalid firmware ring offset')
    return {'rgba_sha256':hashlib.sha256(rgba).hexdigest(),'render':render_values,'sentinel_pixels':sentinel,'firmware_before_after':snapshots,'tile_slots':tile_values,'lifetime':life_values,'fences':fence_values,'shutdown':stop,'firmware_concurrent_update':any(s['write']!=s['write_after'] for s in snapshots),'limitations':['snapshots are not atomic','nonzero slots are raw indices, not proven tile coordinates','zero HWR or pending counts do not prove no recovery','firmware shutdown does not prove DMA quiescence','render success requires separate pixel geometry analysis']}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('uart',type=pathlib.Path);p.add_argument('--offset',type=int,default=0);p.add_argument('--end',type=int)
    a=p.parse_args()
    if a.offset<0 or (a.end is not None and a.end<a.offset): raise ValueError('invalid range')
    with a.uart.open('rb') as f:
        f.seek(a.offset);data=f.read() if a.end is None else f.read(a.end-a.offset)
    print(json.dumps(analyze(data),indent=2))
if __name__=='__main__':
    try: main()
    except (OSError,ValueError,KeyError) as e: print(str(e),file=sys.stderr);sys.exit(1)
