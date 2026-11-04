/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_RENDER_TARGET_H
#define A04_GE8300_RENDER_TARGET_H
#include "firmware_objects.h"
#include "render_hwrt.h"
struct a04_gpu_render_target {
    struct a04_gpu_fw_object common, data;
};
/* Both freelists must be initialized live objects in the same arena. The
 * caller validates and owns every GPU VA/extent in params and the physical
 * pages backing both freelists, synchronizes all DMA objects, and retains
 * them until firmware cleanup and device quiescence. This does not submit. */
int a04_gpu_prepare_render_target(struct a04_gpu_fw_arena *arena,
    const struct a04_gpu_fw_object *local,const struct a04_gpu_fw_object *global,
    const struct a04_render_hwrt_params *params,struct a04_gpu_render_target *out);
#endif
