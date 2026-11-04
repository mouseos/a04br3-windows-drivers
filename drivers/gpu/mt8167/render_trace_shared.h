/* SPDX-License-Identifier: MIT */
#ifndef A04_RENDER_TRACE_SHARED_H
#define A04_RENDER_TRACE_SHARED_H
#include <stddef.h>
#include <stdint.h>
/* Linux pvr_rogue_fwif_sf.h: CSW 15, two arguments. A trace packet is
 * ID, two timestamp words, then arguments (pvr_fw_trace.c). This scan is
 * deliberately only a candidate search, not a trace framing validator. */
#define A04_SHARED_TRACE_ID 0x7002300fu
struct a04_shared_trace_summary {
    uint32_t candidates, store_mask, load_mask, other, different;
};
static inline void a04_shared_trace_scan(const volatile uint32_t *words, size_t count,
                                  struct a04_shared_trace_summary *out)
{
    size_t i;
    for (i=0;i<count;++i) {
        uint32_t a,b;
        if (words[i]!=A04_SHARED_TRACE_ID) continue;
        a=words[(i+3u)%count]; b=words[(i+4u)%count];
        ++out->candidates;
        if (a<=256u && !(a&63u)) out->store_mask|=1u<<(a/64u);
        else ++out->other;
        if (b<=256u && !(b&63u)) out->load_mask|=1u<<(b/64u);
        else ++out->other;
        if (a!=b) ++out->different;
    }
}
#endif
