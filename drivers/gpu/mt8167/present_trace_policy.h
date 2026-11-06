/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GE8300_PRESENT_TRACE_POLICY_H
#define A04_GE8300_PRESENT_TRACE_POLICY_H
#include <stdint.h>
#ifndef A04_GE8300_PRESENT_TRACE_ALL
#define A04_GE8300_PRESENT_TRACE_ALL 0
#endif
/* Count completed presents, saturating at the call site. Never hide failures. */
static inline int a04_gpu_present_trace(uint32_t completed,int32_t status)
{ return A04_GE8300_PRESENT_TRACE_ALL || status<0 || completed<=3u; }
#endif
