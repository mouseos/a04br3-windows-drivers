/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_FRAME_SEQUENCE_H
#define A04_SHARED_FRAME_SEQUENCE_H
#include <stdint.h>
/* Serialized by provider lifetime lock; a failed/partial frame is never ready. */
struct a04_shared_frame_sequence { uint32_t frame,next,failed,sealed,last_task,last_flush; };
static int a04_shared_frame_accept(struct a04_shared_frame_sequence *s,uint32_t frame,uint32_t index)
{
    if(!s || s->sealed || !frame || index>=1000u)return 0;
    if(!index && frame>s->frame){s->frame=frame;s->next=0;s->failed=0;s->last_task=0;s->last_flush=0;}
    return s->frame==frame && s->next==index;
}
static void a04_shared_frame_complete_tile(struct a04_shared_frame_sequence *s,int success)
{ if(s) { if(success && s->next<1000u)s->next++; else {s->next=1000u;s->failed=1;} } }
static int a04_shared_frame_seal(struct a04_shared_frame_sequence *s,uint32_t frame,uint32_t task,uint32_t flush)
{
    if(!s || s->sealed || s->failed || !frame || s->frame!=frame || s->next!=1000u ||
       !task || !flush || task!=s->last_task || flush!=s->last_flush)return 0;
    s->sealed=1;return 1;
}
#endif
