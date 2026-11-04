/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "firmware.h"
#include "render_context.h"
#include "render_pair.h"
#include "render_stream.h"
#include "gpu_render_mesa_fixture.h"
int main(void)
{
    uint8_t *heap=aligned_alloc(4096,A04_PVR_HEAP_SIZE); size_t before;
    struct a04_gpu_fw_arena arena={0}; struct a04_gpu_fw_object vm={0};
    struct a04_gpu_render_context result={0},old={0};
    struct rogue_fwif_geom_registers_caswitch state;
    struct rogue_fwif_fwrendercontext *ctx;
    struct a04_gpu_fw_object hwrt;
    struct rogue_fwif_cmd_geom geom={0}; struct rogue_fwif_cmd_frag frag={0};
    struct rogue_fwif_kccb_cmd kick={0}, empty={0};
    struct rogue_fwif_ccb_cmd_header *header;
    struct rogue_fwif_ufo *ufo;
    struct rogue_fwif_cccb_ctl *ctl;
    uint32_t off;
    assert(heap);
    assert(!a04_gpu_decode_render_static(mesa_render_static,sizeof(mesa_render_static),&state));
    assert(!a04_gpu_fw_arena_init(&arena,heap,A04_PVR_HEAP_SIZE,0x80000000,0x20000));
    assert(!a04_gpu_fw_memory_context(&arena,0x90000000,&vm));
    before=arena.next;
    assert(a04_gpu_prepare_render_context(&arena,&vm,1,0x8000000001ULL,4,&state,&result));
    assert(arena.next==before && !memcmp(&result,&old,sizeof(result)));
    assert(!a04_gpu_prepare_render_context(&arena,&vm,1,0x800006d000ULL,4,&state,&result));
    ctx=result.context.cpu;
    assert(ctx->geom_context.dm==PVR_FWIF_DM_GEOM && ctx->frag_context.dm==PVR_FWIF_DM_FRAG);
    assert(ctx->geom_context.fw_mem_context_fw_addr==vm.fw_addr && ctx->frag_context.fw_mem_context_fw_addr==vm.fw_addr);
    assert(ctx->geom_context.ccb_fw_addr==result.geom_ring.fw_addr && ctx->frag_context.ccb_fw_addr==result.frag_ring.fw_addr);
    assert(ctx->static_render_context_state.ctxswitch_regs[0].geom_reg_vdm_context_state_base_addr==state.geom_reg_vdm_context_state_base_addr);
    assert(((struct rogue_fwif_geom_ctx_state *)result.geom_state.cpu)->geom_core[0].geom_reg_vdm_call_stack_pointer_init==0x800006d000ULL);
    assert(((struct rogue_fwif_cccb_ctl *)result.geom_control.cpu)->wrap_mask==32767);
    assert(!a04_gpu_fw_alloc(&arena,sizeof(struct rogue_fwif_hwrtdata),4096,&hwrt));
    ctl=result.geom_control.cpu; ctl->write_offset=8;
    assert(a04_gpu_prepare_render_pair(&arena,&result,&hwrt,&geom,&frag,1,0,&kick));
    assert(!memcmp(&kick,&empty,sizeof(kick)) && !result.pair_prepared);
    ctl->write_offset=0;
    assert(!a04_gpu_prepare_render_pair(&arena,&result,&hwrt,&geom,&frag,1,0,&kick));
    header=result.geom_ring.cpu;
    assert(header->cmd_type==ROGUE_FWIF_CCB_CMD_TYPE_GEOM && header->cmd_size==sizeof(geom));
    off=sizeof(*header)+header->cmd_size; header=(void *)((uint8_t *)result.geom_ring.cpu+off);
    assert(header->cmd_type==ROGUE_FWIF_CCB_CMD_TYPE_UPDATE);
    ufo=(void *)(header+1); assert(ufo->addr==result.geom_fence.fw_addr && ufo->value==1);
    header=result.frag_ring.cpu; assert(header->cmd_type==ROGUE_FWIF_CCB_CMD_TYPE_FENCE_PR);
    ufo=(void *)(header+1); assert(ufo->addr==result.geom_fence.fw_addr && ufo->value==1);
    off=sizeof(*header)+header->cmd_size; header=(void *)((uint8_t *)result.frag_ring.cpu+off);
    assert(header->cmd_type==ROGUE_FWIF_CCB_CMD_TYPE_FRAG && header->cmd_size==sizeof(frag));
    off+=sizeof(*header)+header->cmd_size; header=(void *)((uint8_t *)result.frag_ring.cpu+off);
    assert(header->cmd_type==ROGUE_FWIF_CCB_CMD_TYPE_UPDATE);
    ufo=(void *)(header+1); assert(ufo->addr==result.frag_fence.fw_addr && ufo->value==1);
    assert(kick.cmd_type==ROGUE_FWIF_KCCB_CMD_COMBINED_GEOM_FRAG_KICK);
    assert(kick.cmd_data.combined_geom_frag_cmd_kick_data.geom_cmd_kick_data.context_fw_addr==result.geom_context_fw);
    assert(kick.cmd_data.combined_geom_frag_cmd_kick_data.frag_cmd_kick_data.context_fw_addr==result.frag_context_fw);
    assert(kick.cmd_data.combined_geom_frag_cmd_kick_data.geom_cmd_kick_data.num_cleanup_ctl==1);
    assert(kick.cmd_data.combined_geom_frag_cmd_kick_data.frag_cmd_kick_data.num_cleanup_ctl==1);
    assert(a04_gpu_prepare_render_pair(&arena,&result,&hwrt,&geom,&frag,2,0,&kick));
    before=arena.next; arena.next=arena.limit-64;
    assert(a04_gpu_prepare_render_context(&arena,&vm,2,0x800006d000ULL,4,&state,&result));
    assert(arena.next==arena.limit-64); arena.next=before;
    free(heap); return 0;
}
