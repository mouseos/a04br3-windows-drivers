/* SPDX-License-Identifier: MIT */
#include "platform_clock.h"
#include "bqhr_diagnostic.h"
#include "charger.h"
static uint64_t get64(const uint8_t *p)
{ uint64_t v=0;unsigned i;for(i=0;i<8;++i)v|=(uint64_t)p[i]<<(i*8);return v; }
enum a04_bq_clock_result a04_bq_clock_compare(const uint8_t *record,unsigned size,uint64_t monotonic_before_ns,
                                               uint64_t before,uint64_t after,uint64_t frequency_before,uint64_t frequency_after)
{
    struct a04_bqhr_info info;
    struct a04_bq_owner candidate={0};
    enum a04_bqhr_result state=a04_bqhr_classify(record,size,&info);
    if (state==A04_BQHR_INVALID || !frequency_before || frequency_before!=frequency_after || after<=before)
        return A04_CLOCK_INVALID;
    if (state==A04_BQHR_STOPPED) return A04_CLOCK_STOPPED;
    /* If these really share a counter, both Windows observations must follow
     * the firmware exit observation; matching alone is never proof. */
    if (before<get64(record+40)) return A04_CLOCK_INCONSISTENT;
    if (a04_bq_adopt_handoff(&candidate,record,size,monotonic_before_ns,after,frequency_after))
        return A04_CLOCK_INCONSISTENT;
    return A04_CLOCK_CANDIDATE;
}
