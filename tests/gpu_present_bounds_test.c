/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "present_bounds.h"
int main(void)
{
    size_t allocation=0; uint32_t span=0;
    uint32_t w=0,h=0,offset=0;
    if (a04_present_bounds(800,1280,4,3328,3,24,4,16,128,&allocation,&span) ||
        span!=1279u*3328u+3200u || allocation!=128u+3u*24u+4u*16u) return 1;
    allocation=123;span=456;
    if (!a04_present_bounds(800,1280,4,3199,0,24,0,16,128,&allocation,&span) ||
        allocation!=123 || span!=456) return 2;
    if (!a04_present_bounds(800,1280,4,-3200,0,24,0,16,128,&allocation,&span) ||
        !a04_present_bounds(0,1280,4,3200,0,24,0,16,128,&allocation,&span) ||
        !a04_present_bounds(1,UINT32_MAX,4,INT32_MAX,0,24,0,16,128,&allocation,&span) ||
        !a04_present_bounds(1,1,4,4,2,SIZE_MAX,0,16,128,&allocation,&span) ||
        !a04_present_bounds(1,1,4,4,0,24,2,SIZE_MAX,128,&allocation,&span)) return 3;
    if (a04_present_rect(0,0,800,1280,800,1280) ||
        a04_present_move(0,0,100,200,700,1080,800,1280) ||
        !a04_present_rect(-1,0,10,10,800,1280) ||
        !a04_present_rect(0,0,801,10,800,1280) ||
        !a04_present_rect(10,10,10,11,800,1280) ||
        !a04_present_move(0,0,100,200,701,1080,800,1280) ||
        !a04_present_move(0,0,100,200,-1,0,800,1280)) return 4;
    if (a04_present_layout(800,1280,1280,800,5120,1,0,&w,&h,&offset) ||
        w!=1280 || h!=800 || offset ||
        a04_present_layout(600,1000,1280,800,5120,3,1,&w,&h,&offset) ||
        w!=1000 || h!=600 || offset!=100u*5120u+140u*4u ||
        !a04_present_layout(800,1280,800,1280,3200,1,0,&w,&h,&offset) ||
        !a04_present_layout(800,1280,800,1280,3199,0,0,&w,&h,&offset) ||
        !a04_present_layout(800,1280,800,1280,3200,4,0,&w,&h,&offset)) return 5;
    if (a04_present_gpu_span(800,1280,800,1280,3200,3200,0,1,800,10,&offset,&span) ||
        offset!=3200u || span!=28800u ||
        !a04_present_gpu_span(800,1280,800,1280,3200,3328,0,1,800,10,&offset,&span) ||
        !a04_present_gpu_span(800,1280,800,1280,3200,3200,0,1,799,10,&offset,&span) ||
        !a04_present_gpu_span(800,1280,800,1280,3200,3200,0,1278,800,1281,&offset,&span) ||
        !a04_present_gpu_span(800,1280,800,1280,3200,3200,0,0,800,4,&offset,&span)) return 6;
    puts("WDDM present pitched span, rectangles and rotated geometry verified"); return 0;
}
