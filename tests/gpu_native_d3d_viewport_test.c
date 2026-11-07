/* SPDX-License-Identifier: MIT. Host float oracle, no GPU or kernel FP. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/native_d3d_viewport.h"
static uint32_t bits(float f){uint32_t n;memcpy(&n,&f,4);return n;}
int main(void)
{
 uint32_t in[6]={0},out[6],old[6];
 for(uint32_t i=0;i<4096;i++){
  uint32_t x=(i*13)%800,y=(i*17)%600,w=1+(i*19)%(800-x),h=1+(i*23)%(600-y);
  in[0]=bits((float)x);in[1]=bits((float)y);in[2]=bits((float)w);in[3]=bits((float)h);in[5]=bits(1);
  assert(!a04_m111_build_d3d_viewport(out,in));
  uint32_t expected[6]={bits((float)w/2),bits((float)x+(float)w/2),bits(-(float)h/2),bits((float)y+(float)h/2),bits(1),0};
  assert(!memcmp(out,expected,sizeof(out)));
 }
 memcpy(old,out,sizeof(old));in[0]=bits(.5f);
 assert(a04_m111_build_d3d_viewport(out,in)!=0 && !memcmp(old,out,sizeof(old)));
 in[0]=bits(799);in[2]=bits(2);assert(a04_m111_build_d3d_viewport(out,in)!=0);
 in[0]=0xff800000;assert(a04_m111_build_d3d_viewport(out,in)!=0);
 puts("bounded D3D viewport:4096 rectangles/odd halves, host float oracle and atomic rejection PASS; no GPU");return 0;
}
