/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_TRIANGLE_TEXTURE_H
#define A04_M111_NATIVE_TRIANGLE_TEXTURE_H
#include <stdint.h>
/* Proven capture05 1x1 sampled profile only: nearest, clamp-to-edge U/V,
 * normalized coordinates, no mipchain/compression, 8-bit four-channel texel.
 * Caller owns backing/descriptor alignment and generates all PDS/USC refs. */
struct a04_m111_triangle_texture {uint8_t sampler[16],image[16],texel[4];};
int a04_m111_build_triangle_texture(struct a04_m111_triangle_texture *,
    uint64_t texel_gpu,const uint8_t rgba[4]);
#endif
