/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/native_triangle_pds.h"
#include "../drivers/gpu/mt8167/vendor_m1_11/native_varying_pds.h"
int main(void)
{
 struct geo_draw_pair_link r={0};struct a04_m111_varying_pds p,old;
 r.size=sizeof(r);r.version=2;r.varying_count=1;r.vs_vtxouts=8;
 r.position.count=4;r.vs_f32_smooth=4;
 r.varying[0]=(struct geo_draw_link_varying){GEO_DRAW_SEMANTIC_COLOR,0,4,32,
   GEO_DRAW_INTERP_SMOOTH,{4,4,0},{0,16,0}};
 /* Independently decode final stream: coeff sets partition the entire
  * common-store extent; Z/W iterators precede the four user components. */
 for(unsigned z=0;z<2;z++)for(unsigned w=0;w<2;w++) {
  r.fs_uses_z=z;r.fs_uses_w=w;r.fs_coeffs=16+4*(z+w);
  r.varying[0].fs_coeff.start=4*(z+w);
  assert(!a04_m111_build_varying_pds(&p,&r));
  unsigned n=z+w+1;
  assert(p.code_dwords==n && p.temps==1 && p.data_dwords==((2*n+3)&~3u));
  for(unsigned i=0;i<n;i++) {
   uint64_t data=p.data[2*i]|((uint64_t)p.data[2*i+1]<<32);
   assert((data>>63)==(i==n-1));
   assert(((data>>54)&255)==i);
   assert(p.code[i]==(0xf0000004u|(i<<8)|((i==n-1)?0x04000000u:0)));
   assert(((data>>24)&3)==3);
   if(i==n-1) {
    assert(((data>>18)&3)==3 && ((data>>16)&1)==1);
    assert((data&255)==2*(z+w) && ((data>>8)&255)==2*(z+w));
   } else {
    assert(((data>>18)&3)==0 && ((data>>16)&1)==0);
    assert((data&255)==((z && i==0)?2*w:0));
   }
  }
  assert(p.output_select==(0x08000000u|(z<<21)|(w<<16)));
  assert(p.varying0==4 && !p.varying1);
  assert(p.pds_varying_size==p.data_dwords/4);
  assert(p.usc_varying_size==(r.fs_coeffs+15)/16);
 }
 old=p;r.varying[0].fs_coeff.count=4;
 assert(a04_m111_build_varying_pds(&p,&r)==GEO_UNSUPPORTED);
 assert(!memcmp(&p,&old,sizeof(p)));
 r.varying[0].fs_coeff.count=16;r.varying[0].interpolation=GEO_DRAW_INTERP_NOPERSPECTIVE;
 r.vs_f32_smooth=0;r.vs_f32_npc=4;
 assert(!a04_m111_build_varying_pds(&p,&r));
 assert(!(p.data[4]&(1u<<16)) && p.varying0==0x40000u);
 r.vs_f32_npc=3;assert(a04_m111_build_varying_pds(&p,&r)==GEO_INVALID);
 struct a04_m111_triangle_vertex_pds vp;
 assert(!a04_m111_build_linked_vertex_pds(&vp,UINT64_C(0xba00000080),UINT64_C(0x8200200000),5));
 assert(vp.data[4]==16 && vp.data[8]==32 && vp.data[24]==8 && vp.data[27]==128);
 assert(a04_m111_build_linked_vertex_pds(&vp,UINT64_C(0xba00000080),(UINT64_C(1)<<40)-95,0));
 assert(a04_m111_build_linked_vertex_pds(&vp,UINT64_C(0xba00000080),UINT64_C(0x8200200000),129));
 puts("native varying PDS: Z/W partition, COLOR4, PPP, NPC and atomic rejection passed");return 0;
}
