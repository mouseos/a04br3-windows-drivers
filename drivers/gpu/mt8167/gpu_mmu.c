/* SPDX-License-Identifier: MIT
 * Entry layout follows pvr_mmu.c and pvr_rogue_mmu_defs.h, Copyright (c)
 * 2023 Imagination Technologies Ltd., available under GPL-2.0-only OR MIT. */
#include "gpu_mmu.h"
#define ADDRESS_LIMIT (UINT64_C(1)<<40)
#define ALLOWED_FLAGS (A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_COHERENT|A04_GPU_MMU_SLC_BYPASS)
static int aligned_address(uint64_t address)
{ return address<ADDRESS_LIMIT && !(address&(A04_GPU_PAGE_SIZE-1u)); }
static int overlaps(const void *a,const void *b)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x<A04_GPU_PAGE_SIZE : x-y<A04_GPU_PAGE_SIZE;
}
int a04_gpu_mmu_map_4k(uint32_t *pc,uint64_t *pd,uint64_t *pt,
    uint64_t pd_dma,uint64_t pt_dma,uint64_t gpu_va,uint64_t dma,
    size_t page_count,uint64_t flags)
{
    size_t i,pc_index,pd_index,pt_index; uint32_t pc_entry; uint64_t pd_entry;
    if (!pc || !pd || !pt || ((uintptr_t)pc&3u) || ((uintptr_t)pd&7u) || ((uintptr_t)pt&7u) ||
        overlaps(pc,pd) || overlaps(pc,pt) || overlaps(pd,pt) ||
        !aligned_address(pd_dma) || !aligned_address(pt_dma) || pd_dma==pt_dma ||
        !aligned_address(gpu_va) || !aligned_address(dma) || !page_count || page_count>512u ||
        (flags&~ALLOWED_FLAGS) || page_count>(ADDRESS_LIMIT-dma)/A04_GPU_PAGE_SIZE ||
        page_count>(ADDRESS_LIMIT-gpu_va)/A04_GPU_PAGE_SIZE) return -1;
    pc_index=(size_t)(gpu_va>>30); pd_index=(size_t)((gpu_va>>21)&511u);
    pt_index=(size_t)((gpu_va>>12)&511u);
    if (page_count>512u-pt_index) return -1;
    pc_entry=(uint32_t)(pd_dma>>8)|1u; pd_entry=pt_dma|1u;
    if ((pc[pc_index] && pc[pc_index]!=pc_entry) || (pd[pd_index] && pd[pd_index]!=pd_entry)) return -1;
    for (i=0;i<page_count;++i) if (pt[pt_index+i]) return -1;
    for (i=0;i<page_count;++i) pt[pt_index+i]=(dma+i*(uint64_t)A04_GPU_PAGE_SIZE)|flags|1u;
    pd[pd_index]=pd_entry; pc[pc_index]=pc_entry;
    return 0;
}
