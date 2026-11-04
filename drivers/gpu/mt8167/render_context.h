/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_RENDER_CONTEXT_H
#define A04_GE8300_RENDER_CONTEXT_H
#include "firmware_objects.h"
#include "pvr_rogue_fwif.h"
struct a04_gpu_render_context {
    struct a04_gpu_fw_object context;
    struct a04_gpu_fw_object geom_control,geom_ring,geom_state,geom_fence;
    struct a04_gpu_fw_object frag_control,frag_ring,frag_state,frag_fence;
    uint32_t geom_context_fw,frag_context_fw;
    uint32_t pair_prepared,frag_end,hwrt_fw;
};
/* Caller validates static register addresses/callstack against GPU-owned VM,
 * installs and synchronizes page tables, syncs allocations, and retains all
 * objects until firmware cleanup plus GPU quiescence. No jobs are submitted. */
int a04_gpu_prepare_render_context(struct a04_gpu_fw_arena *arena,
    const struct a04_gpu_fw_object *memory_context,uint32_t context_id,
    uint64_t callstack_gpu,unsigned isp_store_count,
    const struct rogue_fwif_geom_registers_caswitch *static_state,
    struct a04_gpu_render_context *out);
#endif
