/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * CPU-owned 32x32 BGRA staging for a future m1.11 present backend.
 * Neither surface address is a GPU DMA address.  The caller must serialize
 * against VidPn remap and hold the source mapping for the entire operation.
 */
#ifndef A04_PRESENT_TILE_H
#define A04_PRESENT_TILE_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define A04_PRESENT_TILE_EDGE 32u
#define A04_PRESENT_TILE_ROW 128u
#define A04_PRESENT_TILE_BYTES 4096u
struct a04_present_surface { unsigned char *base; size_t bytes; uint32_t width,height,pitch; };
struct a04_present_tile { uint32_t x,y; };
static inline int a04_present_tile_region(const struct a04_present_surface *s,
    const struct a04_present_tile *tile,size_t *offset)
{
    uint64_t start,end,full;
    if(!s || !tile || !offset || !s->base || !s->width || !s->height ||
       (uint64_t)s->width*4u>s->pitch || tile->x>s->width || tile->y>s->height ||
       s->width-tile->x<32u || s->height-tile->y<32u) return -1;
    full=(uint64_t)(s->height-1u)*s->pitch+(uint64_t)s->width*4u;
    start=(uint64_t)tile->y*s->pitch+(uint64_t)tile->x*4u;
    end=start+31u*(uint64_t)s->pitch+128u;
    if(full>s->bytes || end>full || start>SIZE_MAX || end>SIZE_MAX ||
       (uintptr_t)s->base>UINTPTR_MAX-(size_t)end) return -1;
    *offset=(size_t)start;return 0;
}
/* pack and commit require nonoverlapping stage/surface ranges. commit must be
 * called only after successful device completion, cache synchronization and
 * fence, with all stage bytes back under CPU ownership. The caller performs
 * writes appropriate for its mapping (e.g. WRITE_REGISTER_BUFFER_ULONG for
 * POST framebuffer); this helper computes each row, never maps scanout. */
static inline int a04_present_tile_row(const struct a04_present_surface *s,
    const struct a04_present_tile *tile,uint32_t row,size_t *offset)
{
    size_t start;
    if(row>=32 || a04_present_tile_region(s,tile,&start))return -1;
    *offset=start+(size_t)row*s->pitch;return 0;
}
static inline int a04_present_tile_pack(unsigned char *stage,size_t stage_bytes,
    const struct a04_present_surface *s,const struct a04_present_tile *tile)
{
    size_t start,end;unsigned y;uintptr_t a,b;
    if(!stage || stage_bytes<4096 || a04_present_tile_region(s,tile,&start))return -1;
    end=start+(size_t)31*s->pitch+128; a=(uintptr_t)stage;b=(uintptr_t)s->base;
    if(a>UINTPTR_MAX-4096 || (a<=b ? b-a<4096 : a-b<end))return -1;
    for(y=0;y<32;y++)memcpy(stage+(size_t)y*128,s->base+start+(size_t)y*s->pitch,128);
    return 0;
}
#endif
