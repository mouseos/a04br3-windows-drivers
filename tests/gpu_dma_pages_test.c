/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <stdint.h>
#include <stdio.h>
#include "dma_pages.h"
int main(void)
{
    struct a04_gpu_dma_segment s[]={{0x80000000,2048},{0x80000800,2048},{0xa0000000,8192}};
    uint64_t p[3]={1,2,3};
    if(a04_gpu_dma_pages(s,3,p,3) || p[0]!=0x80000000 || p[1]!=0xa0000000 || p[2]!=0xa0001000) return 1;
    p[0]=1;p[1]=2;p[2]=3;s[1].address++;
    if(!a04_gpu_dma_pages(s,3,p,3) || p[0]!=1 || p[1]!=2 || p[2]!=3) return 2;
    s[1].address--;s[2].length--;
    if(!a04_gpu_dma_pages(s,3,p,3) || p[0]!=1 || p[2]!=3) return 3;
    s[2].length++;s[2].address=UINT64_C(0xfffffff000);
    if(!a04_gpu_dma_pages(s,3,p,3) || p[0]!=1 || p[2]!=3) return 4;
    s[2].address=0xa0000000;
    if(!a04_gpu_dma_pages(s,3,(uint64_t *)(void *)s,3) ||
       !a04_gpu_dma_pages(s,3,p,2) || !a04_gpu_dma_pages(s,3,p,513)) return 5;
    puts("GE8300 scatter/gather page conversion and rejection verified");return 0;
}
