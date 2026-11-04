/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "dma_pages.h"
#define PAGE_SIZE 4096u
#define ADDRESS_LIMIT (UINT64_C(1)<<40)
static int overlap(const void *a,size_t as,const void *b,size_t bs)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return x<=y ? y-x<as : x-y<bs;
}
static int convert(const struct a04_gpu_dma_segment *segments,size_t count,
                   uint64_t *out,size_t page_count)
{
    size_t completed=0; uint64_t next=0; unsigned within=0;
    for(size_t i=0;i<count;++i) {
        uint64_t address=segments[i].address,left=segments[i].length;
        if (!left || address>=ADDRESS_LIMIT || left>ADDRESS_LIMIT-address) return -1;
        while(left) {
            uint64_t amount;
            if (!within) {
                if ((address&(PAGE_SIZE-1u)) || completed>=page_count) return -1;
                if (out) out[completed]=address;
            } else if(address!=next) return -1;
            amount=PAGE_SIZE-within; if(amount>left) amount=left;
            address+=amount; left-=amount; within+=(unsigned)amount; next=address;
            if(within==PAGE_SIZE) { within=0; ++completed; }
        }
    }
    return !within && completed==page_count ? 0 : -1;
}
int a04_gpu_dma_pages(const struct a04_gpu_dma_segment *segments,size_t count,
    uint64_t *pages,size_t page_count)
{
    if(!segments || !pages || !count || !page_count || page_count>512u || count>page_count*PAGE_SIZE ||
       count>SIZE_MAX/sizeof(*segments) || ((uintptr_t)segments&7u) || ((uintptr_t)pages&7u) ||
       overlap(segments,count*sizeof(*segments),pages,page_count*sizeof(*pages))) return -1;
    if(convert(segments,count,NULL,page_count)) return -1;
    return convert(segments,count,pages,page_count);
}
