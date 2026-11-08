#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""実bltfuncs.cxxをホストで実行。Windows SEH/MDLはこの試験の対象外。"""
import argparse, pathlib, subprocess, hashlib, json
p=argparse.ArgumentParser();p.add_argument('--output',required=True);a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[1];o=pathlib.Path(a.output).resolve();o.mkdir(parents=True,exist_ok=True)
src=r/'drivers/gpu/mt8167/wddm-display/bltfuncs.cxx'
stub=r'''#include <cstdint>
#include <cstring>
#include <cassert>
#define CONST const
#define VOID void
#define FALSE false
#define _In_
#define _Out_
#define _In_reads_(n)
#define __try if(true)
#define __except(x) else if(false)
#define NT_ASSERT(x) assert(x)
#define BDD_LOG_ASSERTION1(...) assert(false)
#define BDD_LOG_ERROR2(...)
#define RtlCopyMemory std::memcpy
#define BITS_PER_BYTE 8
using BYTE=uint8_t;using WORD=uint16_t;using ULONG=uint32_t;using LONG=int32_t;
using UINT=uint32_t;using UINT32=uint32_t;using UINT16=uint16_t;using UCHAR=uint8_t;
struct POINT{LONG x,y;};struct RECT{LONG left,top,right,bottom;};
enum D3DKMDT_VIDPN_PRESENT_PATH_ROTATION {D3DKMDT_VPPR_IDENTITY=1,D3DKMDT_VPPR_ROTATE90,D3DKMDT_VPPR_ROTATE180,D3DKMDT_VPPR_ROTATE270};
struct BLT_INFO{void* pBits;UINT Pitch,BitsPerPel;POINT Offset;D3DKMDT_VIDPN_PRESENT_PATH_ROTATION Rotation;UINT Width,Height;};
'''
(o/'bltfuncs-host.cxx').write_text(src.read_text().replace('#include "BDD.hxx"',stub))
harness=r'''#include "bltfuncs-host.cxx"
#include "present_bounds.h"
#include <vector>
#include <cstdio>
int main(){
 for(unsigned t=0;t<4;++t)for(unsigned centered=0;centered<2;++centered)for(unsigned rotate=0;rotate<2;++rotate){
  unsigned lw=(t&1)?3:5,lh=(t&1)?5:3,sw,sh,ct=rotate?t:0;
  assert(!a04_present_source_shape(lw,lh,t,ct,&sw,&sh));
  unsigned fw=5+centered*4,fh=3+centered*4,dp=fw*4+12,sp=sw*4+8,pw,ph,off;
  assert(!a04_present_layout(sw,sh,fw,fh,dp,ct,centered,&pw,&ph,&off));
  std::vector<uint32_t> source((sp/4)*sh+16,0xdecafbad),dst((dp/4)*fh+16,0xcafef00d),expected=dst;
  for(unsigned y=0;y<sh;++y)for(unsigned x=0;x<sw;++x)source[y*(sp/4)+x]=0xff000000+y*32+x;
  BLT_INFO si={source.data(),sp,32,{0,0},D3DKMDT_VPPR_IDENTITY,sw,sh};
  BLT_INFO di={(uint8_t*)dst.data()+off,dp,32,{0,0},(D3DKMDT_VIDPN_PRESENT_PATH_ROTATION)(ct+1),pw,ph};
  // Full refresh, then dirty rectangle and final-source move destination.
  RECT rects[3]={{0,0,(int)sw,(int)sh},{1,1,(int)sw,(int)sh},{0,0,1,1}};
  for(unsigned pass=0;pass<3;++pass){
   if(pass)for(unsigned y=0;y<sh;++y)for(unsigned x=0;x<sw;++x)source[y*(sp/4)+x]+=0x100;
   RECT q=rects[pass];BltBits(&di,&si,1,&q);
   for(int y=q.top;y<q.bottom;++y)for(int x=q.left;x<q.right;++x){
    unsigned px=x,py=y;
    if(ct==1){px=y;py=ph-1-x;}if(ct==2){px=pw-1-x;py=ph-1-y;}if(ct==3){px=pw-1-y;py=x;}
    expected[off/4+py*(dp/4)+px]=source[y*(sp/4)+x];
    // Native absolute touch mapped back through the inverse display transform.
    unsigned ix=px,iy=py;
    if(ct==1){ix=ph-1-py;iy=px;}if(ct==2){ix=pw-1-px;iy=ph-1-py;}if(ct==3){ix=py;iy=pw-1-px;}
    assert(ix==(unsigned)x && iy==(unsigned)y);
   }
   assert(dst==expected); // Includes row padding, centering, trailing guards.
  }
 }
 unsigned w,h,off,turns;size_t n;uint32_t span;
 assert(a04_present_layout(800,1280,800,1280,3200,1,0,&w,&h,&off));
 assert(a04_present_layout(UINT32_MAX,1,800,1280,3200,0,1,&w,&h,&off));
 assert(a04_present_bounds(800,1280,4,-3200,0,0,0,0,0,&n,&span));
 assert(a04_present_turns(254,&turns));
 assert(a04_present_rect(0,0,6,3,5,3));assert(a04_present_move(0,0,2,2,4,0,5,3));
 puts("PASS: actual CPU blitter four rotations, Rotate=0, centered/padded/full/dirty/move-destination, guards and touch inverse");
}
'''
(o/'test.cxx').write_text(harness)
cmd=['g++','-std=c++17','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-Wno-unknown-pragmas','-I'+str(r/'drivers/gpu/mt8167/wddm-display'),str(o/'test.cxx'),'-o',str(o/'test')]
subprocess.run(cmd,check=True);subprocess.run([str(o/'test')],check=True)
(o/'result.json').write_text(json.dumps({'status':'pass','source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'compiler':cmd,'hardware_test':False,'seh_mdl_test':False},indent=2)+'\n')
