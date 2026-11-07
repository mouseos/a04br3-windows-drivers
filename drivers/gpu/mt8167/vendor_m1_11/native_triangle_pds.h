/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_TRIANGLE_PDS_H
#define A04_M111_NATIVE_TRIANGLE_PDS_H
#include <stdint.h>
struct a04_m111_triangle_vertex_pds { uint8_t data[32],code[12]; };
/* Fixed contiguous RGB32 or RG32 fetch, zero USC temps, three vertices max.
 * Code is encoded from source ISA fields. Unused constant words are padding.
 * Caller supplies the actual USC address, DMA base, and backed resource sizes. */
int a04_m111_build_triangle_vertex_pds(struct a04_m111_triangle_vertex_pds *out,
    uint64_t usc_gpu,uint64_t vertex_gpu,uint32_t components,uint32_t stride);
#endif
