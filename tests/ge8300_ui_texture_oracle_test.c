/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include "../drivers/gpu/mt8167/wddm-render/testclient/ui_texture_oracle.h"
int main(void) {
 unsigned inside=0,outside=0,edge=0,x,y,f;unsigned char p[4];
 for(y=0;y<600;y++)for(x=0;x<800;x++){int r=ui_region(x,y);if(r>0)inside++;else if(r<0)outside++;else edge++;}
 assert(inside+outside+edge==480000);assert(inside&&outside&&edge);
 /* Independent signed-area points, then explicit byte orders/tolerance. */
 assert(ui_region(400,200)==1&&ui_region(0,0)==-1&&ui_region(80,60)==0);
 for(f=0;f<2;f++)for(y=0;y<129;y++)for(x=0;x<257;x++) {
  unsigned v=ui_color(x,y);ui_store(p,v,f);
  unsigned got=0xff000000u|((unsigned)p[f?0:2]<<16)|((unsigned)p[1]<<8)|p[f?2:0];assert(got==v);
 }
 assert(ui_color(0,0)==0xff113d6d);assert(!ui_matches(0xff143d6d,0xff113d6d));assert(ui_matches(0xff133d6d,0xff113d6d));
 printf("oracle PASS interior=%u exterior=%u edge_excluded=%u tolerance=2; CPU test only\n",inside,outside,edge);return 0;
}
