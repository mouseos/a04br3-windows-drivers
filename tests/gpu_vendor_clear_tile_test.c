/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "clear_tile.h"
#include "render_resources.h"
int main(void)
{
    struct a04_vendor_clear_tile t,old;struct a04_vendor_clear_tile_request q={128,128,128,64,64};
    memset(&t,0xa5,sizeof(t));old=t;
    assert(!a04_vendor_clear_tile_build(&t,&q));
    assert(t.first==(64u*128u+64u)*4u && t.row_bytes==512u && t.end==t.first+31u*512u+128u);
    assert(t.image.pds[2]==(uint32_t)((A04_VENDOR_RENDER_SCRATCH_VA+t.first)>>2));
    assert((t.image.payload[33]&0x3ffffu)==(127u&0x1fffeu)*2u);
    q.x=97;old=t;assert(a04_vendor_clear_tile_build(&t,&q));assert(!memcmp(&t,&old,sizeof(t)));
    q=(struct a04_vendor_clear_tile_request){800,1280,832,0,0};
    assert(a04_vendor_clear_tile_build(&t,&q));assert(!memcmp(&t,&old,sizeof(t)));
    q=(struct a04_vendor_clear_tile_request){32,32,32,0,0};
    assert(!a04_vendor_clear_tile_build(&t,&q));assert(t.first==0 && t.end==4096);
    return 0;
}
