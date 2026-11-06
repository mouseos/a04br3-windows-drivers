/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_FRAME_SEQUENCE_H
#define A04_SHARED_FRAME_SEQUENCE_H
#include <stdint.h>
/* Serialized by provider lifetime lock; a failed/partial frame is never ready. */
struct a04_shared_frame_sequence { uint32_t frame,next; };
static int a04_shared_frame_accept(struct a04_shared_frame_sequence *s,uint32_t frame,uint32_t index)
{
    if(!s || !frame || index>=1000u)return 0;
    if(!index && frame>s->frame){s->frame=frame;s->next=0;}
    return s->frame==frame && s->next==index;
}
static void a04_shared_frame_complete_tile(struct a04_shared_frame_sequence *s,int success)
{ if(s) { if(success && s->next<1000u)s->next++; else s->next=1000u; } }
#endif
