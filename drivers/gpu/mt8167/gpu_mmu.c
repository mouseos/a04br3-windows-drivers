/* SPDX-License-Identifier: MIT
 * Entry layout follows pvr_mmu.c and pvr_rogue_mmu_defs.h, Copyright (c)
 * 2023 Imagination Technologies Ltd., available under GPL-2.0-only OR MIT. */
#include "gpu_mmu.h"
#define ADDRESS_LIMIT (UINT64_C(1)<<40)
#define ALLOWED_FLAGS (A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_COHERENT|A04_GPU_MMU_SLC_BYPASS)
static int aligned_address(uint64_t address)
{ return address<ADDRESS_LIMIT && !(address&(A04_GPU_PAGE_SIZE-1u)); }
static int overlaps(const void *a,size_t a_size,const void *b,size_t b_size)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x<a_size : x-y<b_size;
}
static int map_4k(uint32_t *pc,uint64_t *pd,uint64_t *pt,
    uint64_t pd_dma,uint64_t pt_dma,uint64_t gpu_va,uint64_t dma,
    const uint64_t *pages,size_t page_count,uint64_t flags,int apply)
{
    size_t i,pc_index,pd_index,pt_index; uint32_t pc_entry; uint64_t pd_entry;
    if (!pc || !pd || !pt || ((uintptr_t)pc&3u) || ((uintptr_t)pd&7u) || ((uintptr_t)pt&7u) ||
        overlaps(pc,A04_GPU_PAGE_SIZE,pd,A04_GPU_PAGE_SIZE) ||
        overlaps(pc,A04_GPU_PAGE_SIZE,pt,A04_GPU_PAGE_SIZE) ||
        overlaps(pd,A04_GPU_PAGE_SIZE,pt,A04_GPU_PAGE_SIZE) ||
        !aligned_address(pd_dma) || !aligned_address(pt_dma) || pd_dma==pt_dma ||
        !aligned_address(gpu_va) || !page_count || page_count>512u ||
        (pages && ((uintptr_t)pages&7u)) ||
        (!pages && (!aligned_address(dma) || page_count>(ADDRESS_LIMIT-dma)/A04_GPU_PAGE_SIZE)) ||
        (flags&~ALLOWED_FLAGS) ||
        page_count>(ADDRESS_LIMIT-gpu_va)/A04_GPU_PAGE_SIZE) return -1;
    if (pages && (overlaps(pages,page_count*sizeof(*pages),pc,A04_GPU_PAGE_SIZE) ||
                  overlaps(pages,page_count*sizeof(*pages),pd,A04_GPU_PAGE_SIZE) ||
                  overlaps(pages,page_count*sizeof(*pages),pt,A04_GPU_PAGE_SIZE))) return -1;
    pc_index=(size_t)(gpu_va>>30); pd_index=(size_t)((gpu_va>>21)&511u);
    pt_index=(size_t)((gpu_va>>12)&511u);
    if (page_count>512u-pt_index) return -1;
    pc_entry=(uint32_t)(pd_dma>>8)|1u; pd_entry=pt_dma|1u;
    if ((pc[pc_index] && pc[pc_index]!=pc_entry) || (pd[pd_index] && pd[pd_index]!=pd_entry)) return -1;
    for (i=0;i<page_count;++i)
        if (pt[pt_index+i] || (pages && !aligned_address(pages[i]))) return -1;
    if (!apply) return 0;
    for (i=0;i<page_count;++i)
        pt[pt_index+i]=(pages ? pages[i] : dma+i*(uint64_t)A04_GPU_PAGE_SIZE)|flags|1u;
    pd[pd_index]=pd_entry; pc[pc_index]=pc_entry;
    return 0;
}
int a04_gpu_mmu_map_4k(uint32_t *pc,uint64_t *pd,uint64_t *pt,
    uint64_t pd_dma,uint64_t pt_dma,uint64_t gpu_va,uint64_t dma,size_t page_count,uint64_t flags)
{ return map_4k(pc,pd,pt,pd_dma,pt_dma,gpu_va,dma,0,page_count,flags,1); }
int a04_gpu_mmu_map_pages_4k(uint32_t *pc,uint64_t *pd,uint64_t *pt,
    uint64_t pd_dma,uint64_t pt_dma,uint64_t gpu_va,const uint64_t *pages,size_t page_count,uint64_t flags)
{ return pages ? map_4k(pc,pd,pt,pd_dma,pt_dma,gpu_va,0,pages,page_count,flags,1) : -1; }
int a04_gpu_mmu_replace_pages_4k(uint64_t *pt,uint64_t gpu_va,
    const uint64_t *pages,size_t page_count,uint64_t flags)
{
    size_t i,index;
    if (!pt || !pages || ((uintptr_t)pt&7u) || ((uintptr_t)pages&7u) ||
        !aligned_address(gpu_va) || !page_count || page_count>512u ||
        page_count>(ADDRESS_LIMIT-gpu_va)/A04_GPU_PAGE_SIZE || (flags&~ALLOWED_FLAGS) ||
        overlaps(pt,A04_GPU_PAGE_SIZE,pages,page_count*sizeof(*pages))) return -1;
    index=(size_t)((gpu_va>>12)&511u);
    if (page_count>512u-index) return -1;
    for (i=0;i<page_count;++i)
        if (!(pt[index+i]&1u) || !aligned_address(pages[i])) return -1;
    for (i=0;i<page_count;++i) pt[index+i]=pages[i]|flags|1u;
    return 0;
}
int a04_gpu_mmu_unmap_4k(uint64_t *pt,uint64_t gpu_va,size_t page_count)
{
    size_t i,index;
    if (!pt || ((uintptr_t)pt&7u) || !aligned_address(gpu_va) || !page_count || page_count>512u ||
        page_count>(ADDRESS_LIMIT-gpu_va)/A04_GPU_PAGE_SIZE) return -1;
    index=(size_t)((gpu_va>>12)&511u);
    if (page_count>512u-index) return -1;
    for (i=0;i<page_count;++i) if (!(pt[index+i]&1u)) return -1;
    for (i=0;i<page_count;++i) pt[index+i]=0;
    return 0;
}
int a04_gpu_mmu_map_brn65273(uint32_t *pc,uint64_t *pd_a,uint64_t *pt_a,
    uint64_t *pd_b,uint64_t *pt_b,uint64_t pd_a_dma,uint64_t pt_a_dma,
    uint64_t pd_b_dma,uint64_t pt_b_dma,uint64_t zero_dma)
{
    const uint64_t va_a=UINT64_C(0xf800000000),va_b=UINT64_C(0xf900000000);
    uint64_t addrs[5]={pd_a_dma,pt_a_dma,pd_b_dma,pt_b_dma,zero_dma};
    void *tables[5]={pc,pd_a,pt_a,pd_b,pt_b}; size_t i,j;
    for(i=0;i<5;++i) {
        if (!tables[i] || !aligned_address(addrs[i])) return -1;
        for(j=0;j<i;++j)
            if (addrs[i]==addrs[j] || overlaps(tables[i],A04_GPU_PAGE_SIZE,tables[j],A04_GPU_PAGE_SIZE)) return -1;
    }
    if (map_4k(pc,pd_a,pt_a,pd_a_dma,pt_a_dma,va_a,zero_dma,0,1,0,0) ||
        map_4k(pc,pd_b,pt_b,pd_b_dma,pt_b_dma,va_b,zero_dma,0,1,0,0)) return -1;
    (void)map_4k(pc,pd_a,pt_a,pd_a_dma,pt_a_dma,va_a,zero_dma,0,1,0,1);
    (void)map_4k(pc,pd_b,pt_b,pd_b_dma,pt_b_dma,va_b,zero_dma,0,1,0,1);
    return 0;
}
