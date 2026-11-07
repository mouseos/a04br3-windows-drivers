/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GCE_OBSERVE_H
#define A04_GCE_OBSERVE_H
#include <stdint.h>
struct a04_gce_snapshot { uint32_t enable,status,irq_status,irq_enable; };
/* The caller proves power, clock, access, and exclusive synchronization.
 * A snapshot does not prove secure-world inactivity or resource ownership. */
static inline int a04_gce_observe_thread0(void *context,int authorized,
    int (*read32)(void *,uint32_t,uint32_t *),struct a04_gce_snapshot *out)
{
    struct a04_gce_snapshot temp;
    if(!out || !read32 || authorized!=1)return -1;
    if(read32(context,0x104u,&temp.enable) || read32(context,0x10cu,&temp.status) ||
       read32(context,0x110u,&temp.irq_status) || read32(context,0x114u,&temp.irq_enable))return -1;
    *out=temp;return 0;
}
#endif
