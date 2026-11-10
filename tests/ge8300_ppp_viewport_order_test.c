/* SPDX-License-Identifier: MIT. Real helper, source-derived PPP emit oracle. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/render_owner/native_d3d_viewport.h"
#ifndef GEO_EXPECT_REVERSED_PPP
static uint32_t bits(float x){uint32_t b;memcpy(&b,&x,4);return b;}
#endif
static float f32(uint32_t b){float x;memcpy(&x,&b,4);return x;}
int main(void){
 uint32_t in[6]={0,0,0x44480000u,0x44160000u,0,0x3f800000u},out[6];
#ifndef GEO_EXPECT_REVERSED_PPP
 uint32_t saved[6];
#endif
 const float input[3][4]={{-.8f,.8f,.5f,1},{.8f,.8f,.5f,1},{0,-.8f,.5f,1}};
 assert(!a04_m111_build_d3d_viewport(out,in));
 /* Mesa pvr_emit_ppp_state emits a0,m0,a1,m1,a2,m2. These are packed
  * DWORDs in draw_job.data+0x274, not an invented alternate GPU convention. */
 for(unsigned i=0;i<3;i++){
  float x=f32(out[0])+f32(out[1])*input[i][0]/input[i][3];
  float y=f32(out[2])+f32(out[3])*input[i][1]/input[i][3];
  float z=f32(out[4])+f32(out[5])*input[i][2]/input[i][3];
#ifdef GEO_EXPECT_REVERSED_PPP
  assert(y<=0);assert(z==1);printf("before vertex%u x%.1f y%.1f z%.1f\n",i,x,y,z);
#else
  assert(x>=0&&x<800&&y>=0&&y<600&&z==.5f);
  assert(y==(i==2?540.0f:60.0f));printf("after vertex%u x%.1f y%.1f z%.1f\n",i,x,y,z);
#endif
 }
#ifndef GEO_EXPECT_REVERSED_PPP
 for(unsigned i=0;i<4096;i++){
  unsigned x=i*13%4096,y=i*17%4096,w=1+i*19%(4096-x),h=1+i*23%(4096-y);
  in[0]=bits((float)x);in[1]=bits((float)y);in[2]=bits((float)w);in[3]=bits((float)h);
  assert(!a04_m111_build_d3d_viewport(out,in));
  uint32_t expected[6]={bits(x+w/2.0f),bits(w/2.0f),bits(y+h/2.0f),bits(-(float)h/2.0f),0,bits(1)};
  if(memcmp(out,expected,sizeof(out))){fprintf(stderr,"i%u x%u y%u w%u h%u\n",i,x,y,w,h);for(unsigned j=0;j<6;j++)fprintf(stderr,"%u %08x %08x\n",j,out[j],expected[j]);return 2;}
 }
 memcpy(saved,out,sizeof(out));in[0]=bits(.5f);assert(a04_m111_build_d3d_viewport(out,in)!=0&&!memcmp(saved,out,sizeof(out)));
 in[0]=bits(4095);in[2]=bits(2);assert(a04_m111_build_d3d_viewport(out,in)!=0&&!memcmp(saved,out,sizeof(out)));
 in[0]=0xff800000u;assert(a04_m111_build_d3d_viewport(out,in)!=0&&!memcmp(saved,out,sizeof(out)));
#endif
 puts("PPP viewport oracle PASS; host geometry only, no GPU raster claim");return 0;
}
