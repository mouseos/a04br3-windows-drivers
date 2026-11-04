/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "firmware.h"
#include "firmware_queue.h"
#include "pvr_rogue_fwif_common.h"
int main(void)
{
    uint8_t *heap=aligned_alloc(4096,A04_PVR_HEAP_SIZE); size_t before;
    struct a04_gpu_fw_arena arena={0}; struct a04_gpu_fw_object vm={0};
    struct a04_gpu_fw_queue q={0}; struct rogue_fwif_kccb_cmd cmd={0};
    struct rogue_fwif_fwcomputecontext *ctx; struct rogue_fwif_cccb_ctl *ctl;
    struct rogue_fwif_ccb_cmd_header *hdr; struct rogue_fwif_ufo *ufo;
    if (!heap) return 1;
    if (a04_gpu_fw_arena_init(&arena,heap,A04_PVR_HEAP_SIZE,0x80000000,0x20000) ||
        a04_gpu_fw_memory_context(&arena,0x90000000,&vm)) return 2;
    before=arena.next;
    if (!a04_gpu_fw_prepare_null_queue(&arena,0,1,1,&q,&cmd) || arena.next!=before) return 3;
    if (a04_gpu_fw_prepare_null_queue(&arena,&vm,1,7,&q,&cmd)) return 4;
    ctx=q.context.cpu; ctl=q.control.cpu; hdr=q.ring.cpu;
    if (ctl->wrap_mask!=32767 || ctx->cdm_context.dm!=PVR_FWIF_DM_CDM ||
        ctx->cdm_context.fw_mem_context_fw_addr!=vm.fw_addr ||
        ctx->cdm_context.ccbctl_fw_addr!=q.control.fw_addr || ctx->cdm_context.ccb_fw_addr!=q.ring.fw_addr ||
        hdr->cmd_type!=ROGUE_FWIF_CCB_CMD_TYPE_NULL || hdr->int_job_ref!=7 || *(uint32_t *)q.fence.cpu) return 5;
    hdr=(void *)((uint8_t *)q.ring.cpu+sizeof(*hdr)); ufo=(void *)((uint8_t *)hdr+sizeof(*hdr));
    if (hdr->cmd_type!=ROGUE_FWIF_CCB_CMD_TYPE_UPDATE || hdr->cmd_size!=sizeof(*ufo) ||
        ufo->addr!=q.fence.fw_addr || ufo->value!=1 || cmd.cmd_type!=ROGUE_FWIF_KCCB_CMD_KICK ||
        cmd.cmd_data.cmd_kick_data.context_fw_addr!=q.context.fw_addr ||
        cmd.cmd_data.cmd_kick_data.client_woff_update!=q.write_offset ||
        cmd.cmd_data.cmd_kick_data.client_wrap_mask_update!=32767) return 6;
    { struct rogue_fwif_cmd_transfer transfer={0}; struct rogue_fwif_fwtransfercontext *tctx;
      transfer.regs.isp_ctl=0x12345678; before=arena.next;
      if(!a04_gpu_fw_prepare_transfer_queue(&arena,&vm,2,8,0,&transfer,&q,&cmd) || arena.next!=before) return 8;
      if(a04_gpu_fw_prepare_transfer_queue(&arena,&vm,2,8,1,&transfer,&q,&cmd)) return 9;
      tctx=q.context.cpu; hdr=q.ring.cpu;
      if(tctx->tq_context.dm!=PVR_FWIF_DM_FRAG || hdr->cmd_type!=ROGUE_FWIF_CCB_CMD_TYPE_TQ_3D ||
         hdr->cmd_size!=sizeof(transfer) || ((struct rogue_fwif_cmd_transfer *)(void *)((uint8_t *)hdr+sizeof(*hdr)))->regs.isp_ctl!=0x12345678) return 10;
      hdr=(void *)((uint8_t *)hdr+sizeof(*hdr)+sizeof(transfer)); ufo=(void *)((uint8_t *)hdr+sizeof(*hdr));
      if(hdr->cmd_type!=ROGUE_FWIF_CCB_CMD_TYPE_UPDATE || ufo->addr!=q.fence.fw_addr || ufo->value!=1) return 11; }
    before=arena.next; arena.next=arena.limit-64;
    if (!a04_gpu_fw_prepare_null_queue(&arena,&vm,2,8,&q,&cmd) || arena.next!=arena.limit-64) return 7;
    arena.next=before; free(heap); puts("GE8300 firmware NULL queue construction passed"); return 0;
}
