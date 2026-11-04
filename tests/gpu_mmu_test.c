/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gpu_mmu.h"
static uint32_t pc[1024];
static uint64_t pd[512], pt[512], pd_b[512], pt_b[512];
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
    memset(pt,0,sizeof(pt));
    { uint64_t pages[]={0x80000000,0xa0000000,0x90000000};
      if(a04_gpu_mmu_map_pages_4k(pc,pd,pt,0x12345000,0x6789a000,0,pages,3,0) ||
         pt[0]!=UINT64_C(0x80000001) || pt[1]!=UINT64_C(0xa0000001) || pt[2]!=UINT64_C(0x90000001)) return 11;
      { uint64_t replacement[]={0xb0000000,0xc0000001,0xd0000000,0xe0000000};
        if(!a04_gpu_mmu_replace_pages_4k(pt,0,replacement,3,0) ||
           pt[0]!=UINT64_C(0x80000001) || pt[1]!=UINT64_C(0xa0000001) || pt[2]!=UINT64_C(0x90000001)) return 18;
        replacement[1]=0xc0000000;
        if(a04_gpu_mmu_replace_pages_4k(pt,0,replacement,3,A04_GPU_MMU_READ_ONLY) ||
           pt[0]!=UINT64_C(0xb0000003) || pt[1]!=UINT64_C(0xc0000003) || pt[2]!=UINT64_C(0xd0000003)) return 19;
        if(!a04_gpu_mmu_replace_pages_4k(pt,0,replacement,4,0) ||
           !a04_gpu_mmu_replace_pages_4k(pt,0,pt,3,0) ||
           !a04_gpu_mmu_replace_pages_4k(pt,0x1ff000,replacement,2,0) ||
           !a04_gpu_mmu_replace_pages_4k(pt,0,replacement,3,0x10) ||
           pt[0]!=UINT64_C(0xb0000003) || pt[2]!=UINT64_C(0xd0000003)) return 20;
      }
      if(!a04_gpu_mmu_unmap_4k(pt,0,4) || !pt[0] || !pt[2]) return 14;
      if(a04_gpu_mmu_unmap_4k(pt,0,3) || pt[0] || pt[1] || pt[2] || !pc[0] || !pd[0]) return 15;
      memset(pc,0,sizeof(pc)); memset(pd,0,sizeof(pd)); memset(pt,0,sizeof(pt)); pages[2]++;
      if(!a04_gpu_mmu_map_pages_4k(pc,pd,pt,0x12345000,0x6789a000,0,pages,3,0) || pc[0] || pd[0] || pt[0]) return 12;
      if(!a04_gpu_mmu_map_pages_4k(pc,pd,pt,0x12345000,0x6789a000,0,pt,3,0)) return 13; }
    memset(pc,0,sizeof(pc)); memset(pd,0,sizeof(pd)); memset(pt,0,sizeof(pt));
    if(a04_gpu_mmu_map_brn65273(pc,pd,pt,pd_b,pt_b,0x10000000,0x10001000,0x10002000,0x10003000,0x20000000) ||
       pc[992]!=0x00100001 || pc[996]!=0x00100021 || pd[0]!=0x10001001 ||
       pd_b[0]!=0x10003001 || pt[0]!=0x20000001 || pt_b[0]!=0x20000001) return 16;
    memset(pc,0,sizeof(pc)); memset(pd,0,sizeof(pd)); memset(pt,0,sizeof(pt));
    memset(pd_b,0,sizeof(pd_b)); memset(pt_b,0,sizeof(pt_b)); pt_b[0]=1;
    if(!a04_gpu_mmu_map_brn65273(pc,pd,pt,pd_b,pt_b,0x10000000,0x10001000,0x10002000,0x10003000,0x20000000) ||
       pc[992] || pd[0] || pt[0]) return 17;
    puts("GE8300 GPU page table encoding and rejected mappings passed");
    return 0;
}
