/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "clear_tile.h"
#include "render_resources.h"
#include <stdint.h>
int a04_vendor_clear_tile_build(struct a04_vendor_clear_tile *out,
                                const struct a04_vendor_clear_tile_request *request)
{
    union { uint64_t alignment; struct a04_vendor_clear_tile value; } aligned_tile;
    struct a04_vendor_clear_tile_request q;
    uint64_t row,extent,first,end,address,csr;
    uintptr_t a,b;
    if (!out || !request || ((uintptr_t)out & 7u)) return 1;
    a=(uintptr_t)out;b=(uintptr_t)request;
    if (a>UINTPTR_MAX-sizeof(*out) || b>UINTPTR_MAX-sizeof(*request) ||
        (a<b+sizeof(*request) && b<a+sizeof(*out))) return 1;
    q=*request;
    if(q.width<32 || q.height<32 || q.pitch_pixels<q.width ||
       (q.pitch_pixels&31u) || (q.x&31u) || (q.y&31u) ||
       q.x>q.width-32u || q.y>q.height-32u || q.pitch_pixels>65536u/4u) return 1;
    row=(uint64_t)q.pitch_pixels*4u;extent=row*q.height;
    if(extent>65536u) return 1;
    first=row*q.y+(uint64_t)q.x*4u;end=first+31u*row+128u;
    if(end>extent || end>65536u) return 1;
    address=A04_VENDOR_RENDER_SCRATCH_VA+first;
    if(address<A04_VENDOR_RENDER_SCRATCH_VA || address>=(UINT64_C(1)<<40) || (address&127u)) return 1;
    if(a04_vendor_clear_image_build(&aligned_tile.value.image,A04_VENDOR_RENDER_SCRATCH_VA,0)) return 1;
    csr=(UINT64_C(2)<<56)|(((uint64_t)(q.pitch_pixels-1u)&UINT64_C(0x1fffe))<<33)|
        (UINT64_C(1)<<23)|(UINT64_C(2)<<26)|(UINT64_C(3)<<29);
    aligned_tile.value.image.payload[32]=(uint32_t)csr;aligned_tile.value.image.payload[33]=(uint32_t)(csr>>32);
    aligned_tile.value.image.pds[2]=(uint32_t)(address>>2);
    aligned_tile.value.image.pds[3]=(uint32_t)(address>>34)|(1u<<19);
    aligned_tile.value.first=(uint32_t)first;aligned_tile.value.end=(uint32_t)end;aligned_tile.value.row_bytes=(uint32_t)row;
    *out=aligned_tile.value;return 0;
}
