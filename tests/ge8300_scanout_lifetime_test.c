/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/scanout_lifetime.h"
int main(void)
{
    struct a04_scanout_lifetime s={0};
    assert(!a04_scanout_bind(&s,0x1000,0xfffff000,0x2000,1,1));
    assert(!a04_scanout_bind(&s,0x1000,0x2000,0x1000,1,0));
    assert(a04_scanout_bind(&s,0x1000,0x2000,0x1000,1,1));
    assert(!a04_scanout_switch_begin(&s));
    assert(a04_scanout_submit(&s,42));
    assert(!a04_scanout_complete(&s,41,1,1));
    assert(!a04_scanout_complete(&s,42,1,0));
    assert(s.phase==A04_SCANOUT_RETAIN);
    assert(a04_scanout_stop_begin(&s));
    assert(!a04_scanout_stop_end(&s,1,0));
    assert(a04_scanout_stop_begin(&s));
    assert(a04_scanout_stop_end(&s,1,1));
    assert(a04_scanout_bind(&s,0x1000,0x2000,0x1000,1,1));
    assert(a04_scanout_submit(&s,43));
    assert(a04_scanout_complete(&s,43,1,1));
    assert(a04_scanout_switch_begin(&s));
    assert(!a04_scanout_switch_end(&s,0));
    assert(!a04_scanout_submit(&s,44));
    assert(a04_scanout_stop_begin(&s));
    assert(a04_scanout_stop_end(&s,1,1));
    assert(a04_scanout_bind(&s,0x1000,0x2000,0x1000,1,1));
    assert(a04_scanout_submit(&s,44));
    assert(a04_scanout_complete(&s,44,1,1));
    assert(a04_scanout_switch_begin(&s));
    assert(a04_scanout_switch_end(&s,1));
    assert(!a04_scanout_submit(&s,45));
    assert(a04_scanout_stop_begin(&s));
    assert(a04_scanout_stop_end(&s,1,1));
    return 0;
}
