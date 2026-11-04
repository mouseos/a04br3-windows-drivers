/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "firmware.h"
#include "firmware_objects.h"
#include "pvr_rogue_fwif.h"
int main(void)
{
    uint8_t *heap=aligned_alloc(4096,A04_PVR_HEAP_SIZE); size_t old;
    struct a04_gpu_fw_arena arena={0}; struct a04_gpu_fw_object obj={0},ctx_obj={0};
    struct rogue_fwif_fwmemcontext *ctx;
    if (!heap) return 1;
    memset(heap,0xaa,A04_PVR_HEAP_SIZE);
    if (a04_gpu_fw_arena_init(&arena,heap,A04_PVR_HEAP_SIZE,0x80000000,0x12345)) return 2;
    if (a04_gpu_fw_alloc(&arena,100,4096,&obj) || obj.cpu!=heap+0x13000 ||
        obj.fw_addr!=0xc0013000u || obj.dma!=0x80013000 || obj.size!=100) return 3;
    for(size_t i=0;i<100;++i) if (((uint8_t *)obj.cpu)[i]) return 4;
    old=arena.next;
    if (!a04_gpu_fw_alloc(&arena,0,64,&obj) || !a04_gpu_fw_alloc(&arena,20,48,&obj) || arena.next!=old) return 5;
    if (!a04_gpu_fw_memory_context(&arena,0x123,&ctx_obj) || arena.next!=old) return 6;
    if (a04_gpu_fw_memory_context(&arena,0x12345000,&ctx_obj)) return 7;
    ctx=ctx_obj.cpu;
    if (ctx->pc_dev_paddr!=0x12345000 || ctx->page_cat_base_reg_set!=ROGUE_FW_BIF_INVALID_PCSET ||
        ctx->breakpoint_addr || ctx->fw_mem_ctx_flags) return 8;
    old=arena.next;
    if (!a04_gpu_fw_alloc(&arena,A04_PVR_HEAP_SIZE,64,&obj) || arena.next!=old) return 9;
    free(heap); puts("GE8300 firmware object allocation and VM context passed"); return 0;
}
