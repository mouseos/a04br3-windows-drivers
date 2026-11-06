/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_CLEAR_SURFACE_H
#define A04_M111_NATIVE_CLEAR_SURFACE_H
#include <stdint.h>
struct a04_m111_clear_surface {
    uint32_t emit[2];
    uint64_t csr[2];
};
/* One linear, single-sample, four-8-bit-lane packed32 RT; output register0,
 * identity lane swizzle, no normalization/gamma/rotation, full extent clip.
 * Corresponds to GLES PBE format-table row 0x22, layout3, force8 input.
 * Colour is already packed; no API colour/format conversion is performed.
 * Ordinary CPU state only: backing, mappings, ownership are caller's job. */
int a04_m111_build_clear_surface(struct a04_m111_clear_surface *out,
    uint64_t output_gpu,uint32_t width,uint32_t height,uint32_t pitch_pixels);
#endif
