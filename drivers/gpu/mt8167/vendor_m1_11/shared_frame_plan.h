/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_FRAME_PLAN_H
#define A04_SHARED_FRAME_PLAN_H
#include <stdint.h>
/* Fixed panel: 25 x 40 complete 32x32 tiles. No clipping or rotation. */
#define A04_SHARED_FRAME_TILES 1000u
struct a04_shared_tile { uint32_t x,y,byte_offset,last_byte; };
static int a04_shared_tile_at(uint32_t index,struct a04_shared_tile *t)
{
    uint32_t x,y,first,last;
    if(!t || index>=A04_SHARED_FRAME_TILES)return 0;
    x=(index%25u)*32u;y=(index/25u)*32u;
    first=y*3200u+x*4u;last=first+31u*3200u+128u;
    if((first&127u) || last>4096000u)return 0;
    t->x=x;t->y=y;t->byte_offset=first;t->last_byte=last;return 1;
}
/* m1.11 rgxccb.c: keep room for a 40-byte padding header; when wrapping,
 * publish the padding and task in one kick. This conservative form only
 * accepts a drained CCB and disallows modulo ambiguity. */
struct a04_tq_ring_plan { uint32_t task_at,next,pad_at,pad_bytes; };
static int a04_tq_ring_plan(uint32_t read,uint32_t write,uint32_t dependency,
    struct a04_tq_ring_plan *p)
{
    const uint32_t size=4096u,packet=296u,header=40u;uint32_t remain;
    if(!p || read>=size || write>=size || dependency>=size ||
       (read&7u) || (write&7u) || (dependency&7u) || read!=write || dependency!=read)return 0;
    p->pad_at=0;p->pad_bytes=0;
    if(write+packet+header<=size){p->task_at=write;p->next=write+packet;return 1;}
    remain=size-write;
    if(remain<header || remain+packet>=size)return 0;
    p->pad_at=write;p->pad_bytes=remain;p->task_at=0;p->next=packet;return 1;
}
#endif
