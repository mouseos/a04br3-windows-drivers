/* SPDX-License-Identifier: MIT
 * Render context object layout based on Linux pvr_context.c and pvr_queue.c,
 * Copyright (c) 2023 Imagination Technologies Ltd., MIT option.
 */
#include "render_context.h"
#include <stddef.h>
#define RENDER_RING_SIZE (1u<<15)
static void init_common(struct rogue_fwif_fwcommoncontext *c,
    const struct a04_gpu_fw_object *control,const struct a04_gpu_fw_object *ring,
    const struct a04_gpu_fw_object *state,const struct a04_gpu_fw_object *vm,
    uint32_t id,uint32_t dm)
{
    c->ccbctl_fw_addr=control->fw_addr; c->ccb_fw_addr=ring->fw_addr;
    c->context_state_addr=state->fw_addr; c->fw_mem_context_fw_addr=vm->fw_addr;
    c->server_common_context_id=id; c->dm=dm; c->priority=1; c->max_deadline_ms=30000;
}
int a04_gpu_prepare_render_context(struct a04_gpu_fw_arena *arena,
    const struct a04_gpu_fw_object *vm,uint32_t id,uint64_t callstack,unsigned isp_count,
    const struct rogue_fwif_geom_registers_caswitch *static_state,struct a04_gpu_render_context *out)
{
    struct a04_gpu_render_context r={0}; struct rogue_fwif_fwrendercontext *ctx;
    struct rogue_fwif_geom_ctx_state *geom; size_t offset,saved;
    if (!arena || !arena->heap || !vm || !out || !static_state || !id || !isp_count || isp_count>64 ||
        !callstack || (callstack&15u) || callstack>=UINT64_C(0x10000000000) ||
        (vm->fw_addr&0xff000000u)!=0xc0000000u || (vm->fw_addr&63u) ||
        vm->size!=sizeof(struct rogue_fwif_fwmemcontext)) return -1;
    offset=(size_t)(vm->fw_addr&0x00ffffffu);
    if (offset>arena->next || vm->size>arena->next-offset || vm->cpu!=arena->heap+offset || vm->dma!=arena->dma+offset) return -1;
    saved=arena->next;
    if (a04_gpu_fw_alloc(arena,sizeof(*ctx),4096,&r.context) ||
        a04_gpu_fw_alloc(arena,sizeof(struct rogue_fwif_cccb_ctl),4096,&r.geom_control) ||
        a04_gpu_fw_alloc(arena,RENDER_RING_SIZE,4096,&r.geom_ring) ||
        a04_gpu_fw_alloc(arena,sizeof(*geom),4096,&r.geom_state) ||
        a04_gpu_fw_alloc(arena,sizeof(uint32_t),4096,&r.geom_fence) ||
        a04_gpu_fw_alloc(arena,sizeof(struct rogue_fwif_cccb_ctl),4096,&r.frag_control) ||
        a04_gpu_fw_alloc(arena,RENDER_RING_SIZE,4096,&r.frag_ring) ||
        a04_gpu_fw_alloc(arena,sizeof(struct rogue_fwif_frag_ctx_state)+isp_count*sizeof(uint32_t),4096,&r.frag_state) ||
        a04_gpu_fw_alloc(arena,sizeof(uint32_t),4096,&r.frag_fence)) { arena->next=saved; return -1; }
    ctx=r.context.cpu;
    init_common(&ctx->geom_context,&r.geom_control,&r.geom_ring,&r.geom_state,vm,id,PVR_FWIF_DM_GEOM);
    init_common(&ctx->frag_context,&r.frag_control,&r.frag_ring,&r.frag_state,vm,id,PVR_FWIF_DM_FRAG);
    ctx->static_render_context_state.ctxswitch_regs[0]=*static_state;
    geom=r.geom_state.cpu; geom->geom_core[0].geom_reg_vdm_call_stack_pointer_init=callstack;
    ((struct rogue_fwif_cccb_ctl *)r.geom_control.cpu)->wrap_mask=RENDER_RING_SIZE-1u;
    ((struct rogue_fwif_cccb_ctl *)r.frag_control.cpu)->wrap_mask=RENDER_RING_SIZE-1u;
    r.geom_context_fw=r.context.fw_addr+(uint32_t)offsetof(struct rogue_fwif_fwrendercontext,geom_context);
    r.frag_context_fw=r.context.fw_addr+(uint32_t)offsetof(struct rogue_fwif_fwrendercontext,frag_context);
    *out=r; return 0;
}
