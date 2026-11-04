/* SPDX-License-Identifier: MIT */
#ifndef A04_WINDOWS_RENDER_DIAGNOSTIC_H
#define A04_WINDOWS_RENDER_DIAGNOSTIC_H
#include "windows_render_memory.h"
#if A04_GE8300_RENDER_EXPERIMENT
/* Fixed, isolated single-shot diagnostic. The result counts are observations;
 * they do not establish pixel positions or prove the image is correct. */
struct a04_windows_render_diagnostic {
    struct a04_windows_render_memory memory;
    uint32_t stage,background_pixels,triangle_pixels,other_pixels;
    NTSTATUS result;
};
NTSTATUS a04_windows_render_diagnostic_run(struct a04_windows_render_diagnostic *d,
    struct a04_gpu_firmware_state *firmware);
#endif
#endif
