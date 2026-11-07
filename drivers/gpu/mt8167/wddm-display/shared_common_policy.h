/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SHARED_COMMON_POLICY_H
#define A04_SHARED_COMMON_POLICY_H
#include <stdint.h>
/* All callers serialize with the present mutex. Retry opening the provider,
 * but issue the common-page lease request at most once per driver lifetime. */
struct a04_shared_common_once { uint32_t attempted; };
static inline int a04_common_may_attempt(const struct a04_shared_common_once *s)
{ return s && !s->attempted; }
static inline int a04_common_claim_attempt(struct a04_shared_common_once *s)
{ if(!a04_common_may_attempt(s))return 0;s->attempted=1;return 1; }
/* The OVL layer consumes one 32-bit linear device address, not CPU PFNs. */
static inline int a04_display_common_range_valid(uint64_t dma,uintptr_t cpu,uint32_t bytes)
{ return cpu && !(cpu&4095u) && dma && !(dma&4095u) && bytes==4096000u && dma<=UINT64_C(0x100000000)-bytes; }
#endif
