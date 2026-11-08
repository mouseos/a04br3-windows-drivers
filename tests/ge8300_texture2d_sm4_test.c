/* SPDX-License-Identifier: MIT. WDK SM4 framing/operands, no PCO/raster claim. */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../drivers/gpu/mt8167/wddm-render/compiler/texture2d_sm4.h"
int main(void){
 const uint32_t good[]={0x40,27,0x04000059,0x00208e46,0,1,0x04001858,0x00107000,0,0x5555,0x0300005a,0x00106000,0,0x03000065,0x001020f2,0,0x0a000045,0x001020f2,0,0x00208046,0,0,0x00107e46,0,0x00106000,0,0x0100003e};
 uint32_t t[27];struct geo_sm4_signatures sig={0};struct geo_texture2d_program p={91,92,93},old=p;
 sig.output_count=1;sig.outputs[0].mask=15;
 assert(!geo_sm4_texture2d_decode(good,sizeof(good),&sig,&p));assert(!p.coordinate_constant&&!p.coordinate_x&&p.coordinate_y==1);
 for(unsigned n=0;n<27;n++){
  memcpy(t,good,sizeof(t));t[1]=n;p=old;
  assert(geo_sm4_texture2d_decode(t,n*4,&sig,&p)!=0&&!memcmp(&p,&old,sizeof(p)));
 }
 #define BAD(index,value) do{memcpy(t,good,sizeof(t));t[index]=value;p=old;assert(geo_sm4_texture2d_decode(t,sizeof(t),&sig,&p)!=0&&!memcmp(&p,&old,sizeof(p)));}while(0)
 BAD(0,0x50);BAD(6,0x04004058);BAD(9,0x4444);BAD(10,0x0300085a);BAD(8,1);BAD(12,1);BAD(16,0x0a000048);BAD(16,0x8a000045);BAD(19,0x00208047);BAD(20,1);BAD(21,1);BAD(22,0x00107e56);BAD(23,1);BAD(25,1);BAD(26,0x0100003f);BAD(5,31);
 sig.input_count=1;p=old;assert(geo_sm4_texture2d_decode(good,sizeof(good),&sig,&p)!=0&&!memcmp(&p,&old,sizeof(p)));
 puts("real bounded SAMPLE2D decoder: CB0 UV/t0 float2D/s0; truncation/LOD/comparison/array/MSAA/modifier/index/type rejection PASS; no default feature enabled");return 0;
}
