/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_ARM_CLOCK_H
#define A04_ARM_CLOCK_H
#include <stdint.h>
/* Convert 100ns ticks to microseconds without the ARM MSVC __rt_udiv64
 * library thunk (WDK8.1 helpexcept.obj requires unavailable GS helpers).
 * Divide the upper word, then carry its remainder through the lower word.
 * Intermediate remainder is <20; no 64-bit division or replacement ABI stub. */
static inline uint64_t a04_interrupt_time_us(uint64_t ticks)
{
 uint32_t high=(uint32_t)(ticks>>32),low=(uint32_t)ticks;
 uint32_t top=high/10u,remainder=high%10u,bottom=0;
 for(unsigned bit=32;bit>0;bit--){
  remainder=(remainder<<1)|((low>>(bit-1))&1u);
  if(remainder>=10u){remainder-=10u;bottom|=1u<<(bit-1);}
 }
 return ((uint64_t)top<<32)|bottom;
}
#endif
