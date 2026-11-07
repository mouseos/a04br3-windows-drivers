/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/gpu/mt8167/wddm-display/gpu_post_pattern.h"
int main(void)
{
    unsigned i,j;
    for(i=0;i<8;i++) {
        uint32_t c=a04_gpu_post_pixel(i*100+50,100);
        assert((c>>24)==0xff);
        for(j=0;j<i;j++)assert(c!=a04_gpu_post_pixel(j*100+50,100));
    }
    assert(a04_gpu_post_pixel(80,320)==0xff000000);
    assert(a04_gpu_post_pixel(719,959)==0xff000000);
    assert(a04_gpu_post_pixel(79,320)!=0xff000000);
    return 0;
}
