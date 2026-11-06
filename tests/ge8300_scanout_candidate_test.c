/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/scanout_candidate.h"
int main(void)
{
    uint64_t pages[1000]; unsigned i;
    for (i=0; i<1000; ++i) pages[i]=UINT64_C(0x80000000)+i*4096u;
    assert(a04_scanout_candidate_check(800,1280,3200,pages[0],pages,1000));
    assert(!a04_scanout_candidate_check(800,1280,3328,pages[0],pages,1000));
    assert(!a04_scanout_candidate_check(800,1280,3200,pages[0],pages,999));
    assert(!a04_scanout_candidate_check(800,1280,3200,UINT64_C(0xfff00000),pages,1000));
    assert(!a04_scanout_candidate_check(800,1280,3200,UINT64_MAX-4095,pages,1000));
    pages[517]+=4096;
    assert(!a04_scanout_candidate_check(800,1280,3200,pages[0],pages,1000));
    return 0;
}
