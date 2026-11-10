/* SPDX-License-Identifier: MIT. Descriptor bitfields and bounds, no GPU. */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_state.h"
static uint64_t qw(const uint32_t *p){return p[0]|(uint64_t)p[1]<<32;}
int main(void){
 struct a04_texture2d_storage s={sizeof(s),1,7,5,32,160,87,1,1,1,0,UINT64_C(0x8200001000)};
 struct a04_texture2d_state out,old;
 assert(!a04_m111_texture2d_state(&out,&s));
 uint64_t a=qw(out.image),b=qw(out.image+2),p=qw(out.sampler);
 assert(((a>>48)&16383)==4&&((a>>34)&16383)==6&&(a&7)==4);
 assert(((a>>27)&127)==12&&((a>>14)&7)==2&&((a>>11)&7)==1&&((a>>8)&7)==0&&((a>>5)&7)==3);
 assert((b&32767)==7&&((b>>16)&UINT64_C(0x3fffffffff))==(s.gpu>>2)&&((b>>60)&15)==1&&((b>>59)&1));
 assert(((p>>41)&7)==2&&((p>>44)&7)==2&&((p>>56)&7)==2&&(p&8191)==4095&&((p>>13)&1023)==0&&((p>>23)&1023)==0);
 assert(!out.sampler[2]&&!out.sampler[3]);old=out;
 #define BAD(field,value) do{struct a04_texture2d_storage bad=s;bad.field=value;assert(a04_m111_texture2d_state(&out,&bad)!=0&&!memcmp(&out,&old,sizeof(out)));}while(0)
 BAD(row_pitch,27);BAD(row_pitch,24);BAD(bytes,159);BAD(gpu,UINT64_C(0xfffffffffc));BAD(gpu,4097);BAD(gpu,0);BAD(width,0);BAD(width,16385);BAD(mip_levels,2);BAD(array_size,2);BAD(sample_count,4);BAD(format,29);BAD(reserved,1);BAD(version,2);BAD(row_pitch,0xfffffffc);
 for(unsigned w=1;w<=128;w++)for(unsigned h=1;h<=128;h++){
  s.width=w;s.height=h;s.row_pitch=w*4;s.bytes=s.row_pitch*h;assert(!a04_m111_texture2d_state(&out,&s));
  a=qw(out.image);b=qw(out.image+2);assert(((a>>48)&16383)==h-1&&((a>>34)&16383)==w-1&&(b&32767)==w-1);
 }
 const uint32_t filters[]={0,1,4,5,16,17,20,21,85};
 for(unsigned k=0;k<9;k++)for(unsigned anis=1;anis<=16;anis++){
  uint32_t f=filters[k],n=f==85?anis:1,ratio=n>=16?4:n>=8?3:n>=4?2:n>=2?1:0;
  assert(!a04_m111_texture2d_state_filtered(&out,&s,f,n));a=qw(out.sampler);
  assert(((a>>38)&3)==((f>>4)&1)&&((a>>36)&3)==((f>>2)&1)&&((a>>40)&1)==(f&1));
  assert(((a>>33)&7)==(f==85?ratio:0)&&((a>>41)&7)==2&&((a>>44)&7)==2&&((a>>56)&7)==2);
 }
 struct a04_texture2d_state saved=out;
 assert(a04_m111_texture2d_state_filtered(&out,&s,0x80,1)&&!memcmp(&out,&saved,sizeof(out)));
 assert(a04_m111_texture2d_state_filtered(&out,&s,0x55,17)&&!memcmp(&out,&saved,sizeof(out)));
 assert(a04_m111_texture2d_state_filtered(&out,&s,0x55,0)&&!memcmp(&out,&saved,sizeof(out)));
 puts("BGRA8 linear2D POINT+CLAMP descriptor:16384 extents/padded pitch/swizzle/atomic invalid rejection PASS; not a texture draw");return 0;
}
