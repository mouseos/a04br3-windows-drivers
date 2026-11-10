/* SPDX-License-Identifier: MIT. Production layout/upload/descriptor, no GPU. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/gpu/mt8167/wddm-render/texture_layout.h"
#include "../drivers/gpu/mt8167/wddm-render/clear_protocol.h"
#include "../drivers/gpu/mt8167/vendor_m1_11/render_owner/native_texture2d_state.h"
static uint64_t qw(const uint32_t*p){return p[0]|((uint64_t)p[1]<<32);}
int main(void){
 /* NPOT UI rows with arbitrary legal upload pitch, padded GPU allocation.
  * Identical byte layout remains BGRA or RGBA; only GPU swizzle changes. */
 const unsigned dims[][2]={{1,1},{3,5},{257,129},{800,600},{800,1280},{1023,511},{16384,1},{1,16384}};
 unsigned cases=0;
 for(unsigned i=0;i<sizeof(dims)/sizeof(*dims);i++)for(unsigned format=0;format<2;format++){
  unsigned w=dims[i][0],h=dims[i][1],pitch=geo_texture_default_pitch(w),srcpitch=w*4+7;
  assert(geo_texture_layout_valid(w,h,pitch));
  struct geo_surface_desc d={sizeof(d),GEO_TEXTURE_STORAGE_VERSION,w,h,pitch,format?GEO_FORMAT_A8B8G8R8:GEO_FORMAT_A8R8G8B8,GEO_USAGE_TEXTURE2D,0};assert(geo_desc_valid(&d));
  struct a04_texture2d_storage s={sizeof(s),1,w,h,pitch,pitch*h,format?28:87,1,1,1,0,UINT64_C(0x8200800000)};
  struct a04_texture2d_state out;assert(!a04_m111_texture2d_state_filtered(&out,&s,0x15,1));
  uint64_t a=qw(out.image),b=qw(out.image+2);
  assert(((a>>34)&16383)==w-1&&((a>>48)&16383)==h-1);
  assert((b&32767)==pitch/4-1&&!(b&(UINT64_C(1)<<15)));
  assert(((a>>14)&7)==(format?0:2)&&((a>>8)&7)==(format?2:0)&&((a>>11)&7)==1&&((a>>5)&7)==3);
  unsigned char*src=malloc((size_t)srcpitch*h),*dst=malloc((size_t)pitch*h);assert(src&&dst);memset(src,0xc7,(size_t)srcpitch*h);memset(dst,0xa5,(size_t)pitch*h);
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w*4;x++)src[(size_t)y*srcpitch+x]=(unsigned char)(x+31*y);
  assert(!geo_texture_upload_rows(dst,pitch,src,srcpitch,w,h));
  for(unsigned y=0;y<h;y++){assert(!memcmp(dst+(size_t)y*pitch,src+(size_t)y*srcpitch,w*4));for(unsigned x=w*4;x<pitch;x++)assert(dst[(size_t)y*pitch+x]==0xa5);}
  struct a04_texture2d_state saved=out;unsigned oldpitch=s.row_pitch;s.row_pitch=131076;s.bytes=s.row_pitch*h;assert(a04_m111_texture2d_state(&out,&s)&&!memcmp(&out,&saved,sizeof(out)));s.row_pitch=oldpitch;s.bytes=s.row_pitch*h;
  assert(geo_texture_upload_rows(dst,pitch,src,w*4-1,w,h));free(src);free(dst);cases++;
 }
 assert(!geo_texture_layout_valid(16385,1,65540));assert(!geo_texture_layout_valid(1,16385,4));
 assert(!geo_texture_layout_valid(16384,65,65536)); /* existing per-MDL4MiB budget */
 assert(geo_texture_layout_valid(1,1,131072));assert(!geo_texture_layout_valid(1,1,131076));
 printf("UI texture: %u actual BGRA/RGBA NPOT/pitched upload+allocation+descriptor cases; no GPU success claim\n",cases);return 0;
}
