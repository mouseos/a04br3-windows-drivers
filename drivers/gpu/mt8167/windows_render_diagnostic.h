/* SPDX-License-Identifier: MIT */
#ifndef A04_WINDOWS_RENDER_DIAGNOSTIC_H
#define A04_WINDOWS_RENDER_DIAGNOSTIC_H
#include "windows_render_memory.h"
#if A04_GE8300_RENDER_EXPERIMENT
/* Fixed single-shot diagnostic; spatial interpretation uses captured 64x64 linear PBE layout. */
struct a04_windows_render_diagnostic {
    struct a04_windows_render_memory memory;
    uint32_t stage,background_pixels,triangle_pixels,other_pixels,zero_pixels,spatial_mismatch,sentinel_pixels;
    uint32_t triangle_rows[64][2], background_rows[64][2], zero_rows[64][2];
    NTSTATUS result;
};
NTSTATUS a04_windows_render_diagnostic_run(struct a04_windows_render_diagnostic *d,
    struct a04_gpu_firmware_state *firmware);
#endif
#endif
