/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../drivers/gpu/mt8167/wddm-display/present_tile.h"
int main(void)
{
    unsigned char src[64*272], stage[4096],dst[64*280];
    struct a04_present_surface s={src,sizeof(src),64,64,272},d={dst,sizeof(dst),64,64,280};
    struct a04_present_tile t={32,32};size_t off;unsigned y,x;
    memset(src,0xcc,sizeof(src));memset(dst,0xa5,sizeof(dst));memset(stage,0,sizeof(stage));
    for(y=0;y<64;y++)for(x=0;x<256;x++)src[y*272+x]=(unsigned char)(x+y);
    assert(!a04_present_tile_pack(stage,sizeof(stage),&s,&t));
    for(y=0;y<32;y++){
      assert(!a04_present_tile_row(&d,&t,y,&off));
      memcpy(dst+off,stage+y*128,128);
      assert(!memcmp(dst+off,src+(y+32)*272+128,128));
      assert(dst[(y+32)*280+127]==0xa5 && dst[(y+32)*280+256]==0xa5);
    }
    t.x=33;assert(a04_present_tile_region(&s,&t,&off));
    t.x=32;t.y=33;assert(a04_present_tile_region(&s,&t,&off));
    t.y=32;s.pitch=255;assert(a04_present_tile_region(&s,&t,&off));
    s.pitch=272;s.bytes=64*272-17;assert(a04_present_tile_region(&s,&t,&off));
    s.bytes=sizeof(src);assert(a04_present_tile_pack(src,sizeof(src),&s,&t));
    assert(a04_present_tile_row(&d,&t,32,&off));
    return 0;
}
