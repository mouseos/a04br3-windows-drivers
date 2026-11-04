/* SPDX-License-Identifier: MIT
 * Firmware object initialization follows Imagination's Linux PowerVR code
 * and UniXNU-a04br3 NativePowerVR.cpp, used under their MIT license. */
#include "firmware_objects.h"
#include "firmware.h"
#include "pvr_rogue_fwif.h"
#define CONFIG_RESERVED (3u*65536u)
#define FW_ALLOCATION_GAP 1048576u
#define FW_BASE UINT32_C(0xc0000000)
int a04_gpu_fw_arena_init(struct a04_gpu_fw_arena *a,void *heap,size_t size,
    uint64_t dma,size_t first)
{
    size_t limit;
    if (!a || !heap || ((uintptr_t)heap&4095u) || size!=A04_PVR_HEAP_SIZE ||
        (dma&4095u) || dma>UINT64_C(0x100000000)-size) return -1;
    limit=size-CONFIG_RESERVED-FW_ALLOCATION_GAP;
    if (first>limit) return -1;
    a->heap=heap; a->size=size; a->next=first; a->limit=limit; a->dma=dma;
    return 0;
}
int a04_gpu_fw_alloc(struct a04_gpu_fw_arena *a,size_t size,size_t alignment,
    struct a04_gpu_fw_object *out)
{
    size_t start,i; struct a04_gpu_fw_object obj;
    if (!a || !a->heap || !out || !size || alignment<64u || alignment>4096u ||
        a->size!=A04_PVR_HEAP_SIZE || a->limit!=a->size-CONFIG_RESERVED-FW_ALLOCATION_GAP ||
        ((uintptr_t)a->heap&4095u) || (a->dma&4095u) || a->dma>UINT64_C(0x100000000)-a->size ||
        (alignment&(alignment-1u)) || a->next>a->limit ||
        a->next>a->limit-(alignment-1u)) return -1;
    start=(a->next+alignment-1u)&~(alignment-1u);
    if (start>a->limit || size>a->limit-start) return -1;
    obj.cpu=a->heap+start; obj.size=size; obj.fw_addr=FW_BASE|(uint32_t)start; obj.dma=a->dma+start;
    for (i=0;i<size;++i) ((uint8_t *)obj.cpu)[i]=0;
    a->next=start+size; *out=obj;
    return 0;
}
int a04_gpu_fw_memory_context(struct a04_gpu_fw_arena *a,uint64_t root_dma,
    struct a04_gpu_fw_object *out)
{
    struct a04_gpu_fw_object obj; struct rogue_fwif_fwmemcontext *ctx;
    if (!out || (root_dma&4095u) || root_dma>=UINT64_C(0x10000000000)) return -1;
    if (a04_gpu_fw_alloc(a,sizeof(*ctx),64u,&obj)) return -1;
    ctx=obj.cpu; ctx->pc_dev_paddr=root_dma; ctx->page_cat_base_reg_set=ROGUE_FW_BIF_INVALID_PCSET;
    *out=obj;
    return 0;
}
