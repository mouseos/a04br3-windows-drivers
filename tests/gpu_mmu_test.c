/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gpu_mmu.h"
static uint32_t pc[1024];
static uint64_t pd[512], pt[512];
int main(void)
{
    uint64_t va=UINT64_C(0x12345000), flags=A04_GPU_MMU_READ_ONLY|A04_GPU_MMU_SLC_BYPASS;
    unsigned ci=(unsigned)(va>>30),di=(unsigned)((va>>21)&511),ti=(unsigned)((va>>12)&511);
    if(a04_gpu_mmu_map_4k(pc,pd,pt,0x12345000,0x6789a000,va,0x80000000,2,flags)) return 1;
    if(pc[ci]!=0x00123451 || pd[di]!=UINT64_C(0x6789a001) ||
       pt[ti]!=(UINT64_C(0x80000000)|flags|1) || pt[ti+1]!=(UINT64_C(0x80001000)|flags|1)) return 2;
    if(!a04_gpu_mmu_map_4k(pc,pd,pt,0x12345000,0x6789a000,va,0x80000000,2,flags)) return 3;
    memset(pc,0,sizeof(pc)); memset(pd,0,sizeof(pd)); memset(pt,0,sizeof(pt));
    if(!a04_gpu_mmu_map_4k(pc,pd,pt,0x12345000,0x6789a000,0x1ff000,0x80000000,2,0) || pc[0] || pd[0] || pt[511]) return 4;
    if(!a04_gpu_mmu_map_4k(pc,pd,pt,0x12345000,0x6789a000,0,UINT64_C(0xfffffff000),2,0)) return 5;
    if(!a04_gpu_mmu_map_4k(pc,pd,pt,0x12345000,0x6789a000,0,0x80000001,1,0)) return 6;
    if(!a04_gpu_mmu_map_4k(pc,pd,pt,0x12345000,0x6789a000,0,0x80000000,1,0x10)) return 7;
    pt[1]=1;
    if(!a04_gpu_mmu_map_4k(pc,pd,pt,0x12345000,0x6789a000,0,0x80000000,2,0) || pc[0] || pd[0] || pt[0]) return 8;
    if(!a04_gpu_mmu_map_4k(pc,(uint64_t *)(void *)pc,pt,0x12345000,0x6789a000,0,0x80000000,1,0)) return 9;
    if(!a04_gpu_mmu_map_4k(pc,pd,pt,0x12345000,0x12345000,0,0x80000000,1,0)) return 10;
    puts("GE8300 GPU page table encoding and rejected mappings passed");
    return 0;
}
