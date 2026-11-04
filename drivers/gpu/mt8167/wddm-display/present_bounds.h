/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_PRESENT_BOUNDS_H
#define A04BR3_PRESENT_BOUNDS_H
#include <stdint.h>
#include <stddef.h>
/* Validate an upright source's full pitched span and the control allocation.
 * Negative/zero stride needs separate bottom-up support and is rejected. */
static inline int a04_present_bounds(uint32_t width,uint32_t height,uint32_t bytes,
    int32_t pitch,uint32_t moves,size_t move_size,uint32_t rects,size_t rect_size,
    size_t context_size,size_t *allocation,uint32_t *mapped_span)
{
    uint64_t row,span; size_t n;
    if (!width || !height || !bytes || pitch<=0 || !allocation || !mapped_span) return -1;
    row=(uint64_t)width*bytes;
    if (row>(uint32_t)pitch) return -1;
    span=(uint64_t)(height-1u)*(uint32_t)pitch+row;
    if (span>UINT32_MAX) return -1;
    n=context_size;
    if (moves && move_size>(SIZE_MAX-n)/moves) return -1;
    n+=(size_t)moves*move_size;
    if (rects && rect_size>(SIZE_MAX-n)/rects) return -1;
    n+=(size_t)rects*rect_size;
    *allocation=n; *mapped_span=(uint32_t)span; return 0;
}
#endif
