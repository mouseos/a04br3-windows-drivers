/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_TRIANGLE_PPP_H
#define A04_M111_NATIVE_TRIANGLE_PPP_H
#include <stdint.h>
#include <stddef.h>
#define A04_M111_TRIANGLE_PREFIX_BYTES 48u
#define A04_M111_TRIANGLE_SOLID_PPP_BYTES 104u
#define A04_M111_TRIANGLE_TEXTURE_PPP_BYTES 108u
/* Captured 800x600 profile, not a general pipeline state interface.
 * All PDS values are offsets from the PDS execution heap, never CPU pointers.
 * Allocation/mapping, shaders, DMA data and actual submission are separate. */
struct a04_m111_triangle_ppp_input {
    uint32_t fragment_pds,texuni_code,varying_pds,uniform_pds,texture_pds;
    uint16_t scissor_index;
    uint8_t textured;
};
int a04_m111_build_triangle_prefix(void *out,size_t bytes);
int a04_m111_build_triangle_ppp(void *out,size_t bytes,
    const struct a04_m111_triangle_ppp_input *input);
#endif
