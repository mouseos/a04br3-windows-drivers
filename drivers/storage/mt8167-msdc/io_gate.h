/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_MSDC_IO_GATE_H
#define MT8167_MSDC_IO_GATE_H
#ifdef _WIN32
typedef volatile LONG a04_msdc_gate;
static int a04_msdc_gate_acquire(a04_msdc_gate *gate)
{ return InterlockedCompareExchange(gate,1,0)==0; }
static void a04_msdc_gate_release(a04_msdc_gate *gate)
{ (void)InterlockedExchange(gate,0); }
#else
#include <stdatomic.h>
typedef atomic_int a04_msdc_gate;
static int a04_msdc_gate_acquire(a04_msdc_gate *gate)
{ int expected=0; return atomic_compare_exchange_strong(gate,&expected,1); }
static void a04_msdc_gate_release(a04_msdc_gate *gate)
{ atomic_store(gate,0); }
#endif
#endif
