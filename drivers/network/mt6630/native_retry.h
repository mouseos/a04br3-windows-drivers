/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_RETRY_H
#define A04_NATIVE_RETRY_H
#include "native_frame.h"
/* Single associated AP, non-HT ERP, no BA/reorder/fragment/A-MSDU admission.
 * CT3 nicRxIsDuplicateFrame: non-HT QoS uses UP->AC sequence cache,
 * non-QoS a separate cache. No user/AP identity or packet bytes retained.
 * Caller validates HIF/AP/key/PN/port first and serializes this with RX slots. */
struct a04_native_retry {uint32_t epoch,valid;uint16_t sequence[5];};
struct a04_native_retry_ticket {uint32_t epoch;unsigned index;uint16_t sequence;};
static inline int a04_native_retry_check(const struct a04_native_retry*s,uint32_t epoch,
 const uint8_t*p,size_t n,struct a04_native_retry_ticket*t)
{
 static const unsigned ac[8]={0,1,1,0,2,2,3,3};struct a04_native_frame f;unsigned index;
 if(!s||!t||a04_native_frame_parse(p,n,2,&f))return -1;
 /* Group transmissions have no individual ACK/retry exchange. */
 if((p[4]&1)&&(p[1]&8))return -1;
 index=(p[4]&1)?5:f.qos?ac[f.tid]:4;
 t->epoch=epoch;t->index=index;t->sequence=(uint16_t)((unsigned)p[22]|(unsigned)p[23]<<8);
 return index<5&&(p[1]&8)&&s->epoch==epoch&&(s->valid&(1u<<index))&&s->sequence[index]==t->sequence?1:0;
}
/* Commit only after the validated MPDU was copied into its owned RX slot.
 * Queue/filter rejection does not pretend the first transmission arrived. */
static inline void a04_native_retry_commit(struct a04_native_retry*s,const struct a04_native_retry_ticket*t)
{
 if(s->epoch!=t->epoch){s->epoch=t->epoch;s->valid=0;}
 if(t->index<5){s->sequence[t->index]=t->sequence;s->valid|=1u<<t->index;}
}
#endif
