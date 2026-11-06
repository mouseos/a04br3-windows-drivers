/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "gce_clock_lease.h"
#define GCE_BIT (1u<<4)
#define TOP2_STATUS 0x3cu
#define TOP2_SET 0x6cu
#define TOP2_CLEAR 0x9cu
int a04_gce_clock_acquire(struct a04_gce_clock_lease *o,uint32_t *token)
{
    uint32_t state;
    if(!o || !token)return -1;
    *token=0;
    if(!o->online || o->ownership_proven!=1 || o->active || o->poisoned ||
       !o->read32 || !o->write32 || o->generation==UINT32_MAX)return -1;
    if(o->read32(o->context,TOP2_STATUS,&state))return -1;
    o->active=++o->generation;*token=o->active;
    o->restore_gated=(state&GCE_BIT)!=0;
    if(o->restore_gated && o->write32(o->context,TOP2_CLEAR,GCE_BIT))goto poison;
    if(o->read32(o->context,TOP2_STATUS,&state) || (state&GCE_BIT))goto poison;
    return 0;
poison:
    o->poisoned=1;return -1;
}
int a04_gce_clock_release(struct a04_gce_clock_lease *o,uint32_t token,int quiescent_proven)
{
    uint32_t state;
    if(!o || !token || token!=o->active || quiescent_proven!=1 || o->poisoned ||
       !o->read32 || !o->write32)return -1;
    if(o->restore_gated) {
        if(o->write32(o->context,TOP2_SET,GCE_BIT) ||
           o->read32(o->context,TOP2_STATUS,&state) || !(state&GCE_BIT)) {
            o->poisoned=1;return -1;
        }
    }
    o->active=0;o->restore_gated=0;return 0;
}
