/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_DISPLAY_BARRIER356_H
#define A04_DISPLAY_BARRIER356_H
static __forceinline void display_barrier356(void){
#if defined(__clang__) && defined(_ARM_)
 __asm__ volatile ("" ::: "memory");__builtin_arm_dmb(15);__asm__ volatile ("" ::: "memory");
#else
 KeMemoryBarrier();
#endif
}
#endif
