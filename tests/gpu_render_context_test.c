/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "firmware.h"
#include "render_context.h"
#include "render_stream.h"
#include "gpu_render_mesa_fixture.h"
int main(void)
{
    uint8_t *heap=aligned_alloc(4096,A04_PVR_HEAP_SIZE); size_t before;
    struct a04_gpu_fw_arena arena={0}; struct a04_gpu_fw_object vm={0};
    struct a04_gpu_render_context result={0},old={0};
    struct rogue_fwif_geom_registers_caswitch state;
    struct rogue_fwif_fwrendercontext *ctx;
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
    before=arena.next; arena.next=arena.limit-64;
    assert(a04_gpu_prepare_render_context(&arena,&vm,2,0x800006d000ULL,4,&state,&result));
    assert(arena.next==arena.limit-64); arena.next=before;
    free(heap); return 0;
}
