/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_OVL_SAVED_H
#define A04_MT8167_OVL_SAVED_H
#include <stdint.h>
/* A04BR3 DisplayInitDxe reserves 16 MiB and programs these two 800x1280
 * layers. Readbacks are consistency checks, not a display lease, an atomic
 * latch, or proof that other firmware/secure agents have stopped. */
#define A04_OVL_SNAPSHOT_WORDS 13u
static inline int a04_ovl_saved_layout(const uint32_t *a,const uint32_t *b,
                                       uint32_t saved0,uint32_t saved3,uint32_t target)
{
    unsigned i;uint64_t end;
    if(!a || !b || !target || (target&3u))return 0;
    for(i=0;i<A04_OVL_SNAPSHOT_WORDS;i++)if(a[i]!=b[i])return 0;
    if(a[0]!=1u || a[1]!=9u || a[2]!=0x05000320u ||
       a[3]!=0x000020ffu || a[4]!=0x05000320u || a[5]!=0u || a[6]!=0x00ff0c80u ||
       a[7]!=0xbe9c0000u || a[8]!=0x000021ffu || a[9]!=0x05000320u ||
       a[10]!=0u || a[11]!=0x00ff0c80u || a[12]!=0xbeda8000u ||
       saved0!=a[7] || saved3!=a[12])return 0;
    /* All compared addresses are in the OVL logical address domain. Reject
     * overlap with the entire reserved old-display range, conservatively. */
    end=(uint64_t)target+4096000u;
    if(end>UINT64_C(0x100000000) || ((uint64_t)target<UINT64_C(0xbf9c0000) && end>UINT64_C(0xbe9c0000)))return 0;
    return 1;
}
#endif
