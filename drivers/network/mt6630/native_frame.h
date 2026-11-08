/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_FRAME_H
#define A04_NATIVE_FRAME_H
#include <stdint.h>
#include <stddef.h>
#include "native_limits.h"
/* IEEE802.11-2012 8.2.4 / board gen3 mac.h: Data24/QoS26/QoS+HTC30.
 * No A4, fragments, HCCA or A-MSDU. FromDS retry requires a caller-owned
 * duplicate cache; parsing alone never authorizes an RX indication.
 * Return a borrowed view only; do not change HIF or OS-owned bytes. */
struct a04_native_frame {unsigned header,tid,qos;};
static inline int a04_native_frame_parse(const uint8_t *p,size_t n,unsigned ds,struct a04_native_frame *v)
{
 unsigned qos,h;
 if(!p||!v||n<24||n>A04_NATIVE_MPDU_MAX||(p[0]!=8&&p[0]!=0x88)||(p[1]&3)!=ds||(p[1]&(ds==2?4u:12u))||(p[22]&15))return -1;
 qos=p[0]==0x88;h=qos?26:24;
 if(p[1]&0x80){if(!qos)return -1;h+=4;}
 if(n<h+8||n-h-8>A04_NATIVE_MTU)return -1;
 /* EDCA UP0..7, normal ACK only. EOSP is not an aggregation request. */
 if(qos&&((p[24]&0xef)>7))return -1;
 for(unsigned i=0;i<6;i++)if(p[h+i]!=(i<2?0xaa:i==2?3:0))return -1;
 v->header=h;v->qos=qos;v->tid=qos?(p[24]&7):0;return 0;
}
/* Native ExtSTA allows removing QoS Control with priority returned OOB.
 * Independent owned destination, sized >= n-header+24. */
static inline size_t a04_native_frame_copy(uint8_t *out,const uint8_t *p,size_t n,unsigned h)
{
 for(unsigned i=0;i<24;i++)out[i]=p[i];
 out[0]=8;out[1]&=0x3f;
 for(size_t i=h;i<n;i++)out[24+i-h]=p[i];
 return n-h+24;
}
#endif
