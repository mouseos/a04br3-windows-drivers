/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "gpu_mmu.h"
static uint32_t pc[1024];
static uint64_t pd[512],pt[3][512],pages[1040];
int main(void)
{
    struct a04_gpu_mmu_leaf leaf[3]={{pt[0],0x220000},{pt[1],0x221000},{pt[2],0x222000}};
    uint64_t va=UINT64_C(0x8000000000);size_t i;
    for(i=0;i<1040;i++)pages[i]=0x40000000u+i*4096u;
    /* A failure in the final table must leave all earlier tables untouched. */
    pt[2][10]=1;
    assert(a04_gpu_mmu_map_pages_multi_4k(pc,pd,0x210000,leaf,3,va,pages,1040,0));
    assert(!pc[va>>30] && !pd[0] && !pt[0][0] && !pt[1][0] && pt[2][10]==1);
    pt[2][10]=0;
    assert(!a04_gpu_mmu_map_pages_multi_4k(pc,pd,0x210000,leaf,3,va,pages,1040,0));
    assert((pc[va>>30]&1u) && pd[0]==(leaf[0].dma|1u) && pd[2]==(leaf[2].dma|1u));
    assert(pt[0][0]==(pages[0]|1u) && pt[1][0]==(pages[512]|1u) && pt[2][15]==(pages[1039]|1u));
    memset(pc,0,sizeof(pc));memset(pd,0,sizeof(pd));memset(pt,0,sizeof(pt));
    leaf[2].dma=leaf[0].dma;
    assert(a04_gpu_mmu_map_pages_multi_4k(pc,pd,0x210000,leaf,3,va,pages,1040,0));
    assert(!pc[va>>30] && !pd[0] && !pt[0][0]);
    leaf[2].dma=0x222000;
    assert(a04_gpu_mmu_map_pages_multi_4k(pc,pd,0x210000,leaf,3,UINT64_C(0x803ffff000),pages,1040,0));
    assert(!pc[va>>30] && !pd[0] && !pt[0][0]);
    return 0;
}
