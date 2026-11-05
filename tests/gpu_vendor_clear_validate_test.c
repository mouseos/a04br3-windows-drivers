/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include "clear_validate.h"
int main(void)
{
    uint32_t i,count=0,guards=0,first=(64u*128u+64u)*4u;
    assert(a04_vendor_clear_layout_valid(first,512));
    for(i=0;i<65536u;i+=4) {
        int target=a04_vendor_clear_is_target(i,first,512);
        int oracle=(i/512u>=64u && i/512u<96u && i%512u>=256u && i%512u<384u);
        assert(target==oracle);if(target)count++;else guards++;
    }
    assert(count==1024 && guards==15360);
    assert(!a04_vendor_clear_layout_valid(65532u,128));
    assert(!a04_vendor_clear_layout_valid(0,0));
    assert(a04_vendor_clear_layout_valid(0,128));
    return 0;
}
