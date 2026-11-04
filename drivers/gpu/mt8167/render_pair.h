/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_RENDER_PAIR_H
#define A04_GE8300_RENDER_PAIR_H
#include "render_context.h"
#include "pvr_rogue_fwif_client.h"
/* Prepare the first single geometry+fragment pair on an otherwise idle
 * context. Internal trusted callers only: all command registers, referenced
 * VA ranges, buffer extents and cache ownership must be validated separately.
 * The HWRT object must belong to the same live firmware arena. The caller
 * synchronizes all objects, submits the returned KCCB kick, waits for both
 * fences and retains DMA until firmware cleanup and GPU quiescence. */
int a04_gpu_prepare_render_pair(const struct a04_gpu_fw_arena *arena,
    struct a04_gpu_render_context *context,const struct a04_gpu_fw_object *hwrt,
    const struct rogue_fwif_cmd_geom *geom,const struct rogue_fwif_cmd_frag *frag,
    uint32_t job_id,int partial_render,struct rogue_fwif_kccb_cmd *kick);
/* Before submitting either kick, append the final fragment after a partial
 * fragment pair. Caller synchronizes the complete ring before the first kick,
 * submits the two kicks in order, and keeps all allocations until cleanup. */
int a04_gpu_prepare_final_fragment(const struct a04_gpu_fw_arena *arena,
    struct a04_gpu_render_context *context,const struct a04_gpu_fw_object *hwrt,
    const struct rogue_fwif_cmd_frag *frag,uint32_t job_id,
    struct rogue_fwif_kccb_cmd *kick);
#endif
