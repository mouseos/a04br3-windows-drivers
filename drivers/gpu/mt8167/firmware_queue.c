/* SPDX-License-Identifier: MIT
 * Based on UniXNU NativePowerVRQueue.cpp and Imagination pvr_queue.c/
 * pvr_cccb.c, Copyright (c) 2023 Imagination Technologies Ltd., MIT option. */
#include "firmware_queue.h"
#include "pvr_rogue_fwif_common.h"
#include <stddef.h>
#define RING_SIZE (1u<<15)
_Static_assert(sizeof(struct rogue_fwif_ufo)==8,"firmware UFO size");
_Static_assert(sizeof(struct rogue_fwif_ccb_cmd_header)%8==0,"firmware command alignment");
static void zero(void *p,size_t n) { size_t i; for(i=0;i<n;++i) ((uint8_t *)p)[i]=0; }
int a04_gpu_fw_prepare_null_queue(struct a04_gpu_fw_arena *arena,const struct a04_gpu_fw_object *mem_context,
    uint32_t context_id,uint32_t job_id,struct a04_gpu_fw_queue *out,struct rogue_fwif_kccb_cmd *kick)
{
    struct a04_gpu_fw_queue q; struct rogue_fwif_cccb_ctl *ctl;
    struct rogue_fwif_fwcomputecontext *ctx; struct rogue_fwif_ccb_cmd_header *hdr;
    struct rogue_fwif_ufo *ufo; struct rogue_fwif_kccb_cmd cmd; size_t saved,off,vm_off;
    if (!arena || !arena->heap || !mem_context || !out || !kick || !context_id || !job_id ||
        (mem_context->fw_addr&0xff000000u)!=0xc0000000u || (mem_context->fw_addr&63u) ||
        mem_context->size!=sizeof(struct rogue_fwif_fwmemcontext)) return -1;
    vm_off=(size_t)(mem_context->fw_addr&0x00ffffffu);
    if (vm_off>arena->next || mem_context->size>arena->next-vm_off ||
        mem_context->cpu!=arena->heap+vm_off || mem_context->dma!=arena->dma+vm_off) return -1;
    saved=arena->next; zero(&q,sizeof(q)); zero(&cmd,sizeof(cmd));
    if (a04_gpu_fw_alloc(arena,sizeof(*ctl),4096,&q.control) ||
        a04_gpu_fw_alloc(arena,RING_SIZE,4096,&q.ring) ||
        a04_gpu_fw_alloc(arena,sizeof(struct rogue_fwif_compute_ctx_state),4096,&q.state) ||
        a04_gpu_fw_alloc(arena,sizeof(uint32_t),4096,&q.fence) ||
        a04_gpu_fw_alloc(arena,sizeof(*ctx),4096,&q.context)) { arena->next=saved; return -1; }
    q.wrap_mask=RING_SIZE-1u; ctl=q.control.cpu; ctl->wrap_mask=q.wrap_mask;
    ctx=q.context.cpu; ctx->cdm_context.ccbctl_fw_addr=q.control.fw_addr;
    ctx->cdm_context.ccb_fw_addr=q.ring.fw_addr; ctx->cdm_context.dm=PVR_FWIF_DM_CDM;
    ctx->cdm_context.priority=1; ctx->cdm_context.max_deadline_ms=30000;
    ctx->cdm_context.server_common_context_id=context_id;
    ctx->cdm_context.fw_mem_context_fw_addr=mem_context->fw_addr;
    ctx->cdm_context.context_state_addr=q.state.fw_addr;
    hdr=q.ring.cpu; hdr->cmd_type=ROGUE_FWIF_CCB_CMD_TYPE_NULL;
    hdr->ext_job_ref=job_id; hdr->int_job_ref=job_id;
    off=sizeof(*hdr); hdr=(void *)((uint8_t *)q.ring.cpu+off);
    hdr->cmd_type=ROGUE_FWIF_CCB_CMD_TYPE_UPDATE; hdr->cmd_size=sizeof(*ufo);
    ufo=(void *)((uint8_t *)hdr+sizeof(*hdr)); ufo->addr=q.fence.fw_addr; ufo->value=1;
    q.write_offset=(uint32_t)(off+sizeof(*hdr)+sizeof(*ufo));
    cmd.cmd_type=ROGUE_FWIF_KCCB_CMD_KICK;
    cmd.cmd_data.cmd_kick_data.context_fw_addr=q.context.fw_addr+(uint32_t)offsetof(struct rogue_fwif_fwcomputecontext,cdm_context);
    cmd.cmd_data.cmd_kick_data.client_woff_update=q.write_offset;
    cmd.cmd_data.cmd_kick_data.client_wrap_mask_update=q.wrap_mask;
    *out=q; *kick=cmd; return 0;
}
