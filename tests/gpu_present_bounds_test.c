/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "present_bounds.h"
int main(void)
{
    size_t allocation=0; uint32_t span=0;
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
    puts("WDDM present pitched span and allocation bounds verified"); return 0;
}
