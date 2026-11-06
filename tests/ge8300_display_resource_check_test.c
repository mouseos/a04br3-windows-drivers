/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/gpu/mt8167/wddm-display/ge8300_display_resource_check.h"
int main(void)
{
    assert(a04_display_resources_valid(1,1,1,1,0x14007000,0x1000));
    assert(a04_display_resources_valid(1,0,1,0,0x14007000,0x1000));
    assert(!a04_display_resources_valid(0,1,1,1,0x14007000,0x1000));
    assert(!a04_display_resources_valid(2,1,1,1,0x14007000,0x1000));
    assert(!a04_display_resources_valid(1,2,1,1,0x14007000,0x1000));
    assert(!a04_display_resources_valid(1,1,0,1,0x14007000,0x1000));
    assert(!a04_display_resources_valid(1,1,1,0,0x14007000,0x1000));
    assert(!a04_display_resources_valid(1,1,1,1,0x14009000,0x1000));
    assert(!a04_display_resources_valid(1,1,1,1,0x14007000,0xfff));
    assert(a04_gce_resources_valid(1,2,1,1,0x1020a000,0x1000));
    assert(!a04_gce_resources_valid(1,1,1,1,0x1020a000,0x1000));
    assert(!a04_gce_resources_valid(1,2,1,0,0x1020a000,0x1000));
    assert(!a04_gce_resources_valid(2,2,1,1,0x1020a000,0x1000));
    assert(!a04_gce_resources_valid(1,2,1,1,0x1020b000,0x1000));
    return 0;
}
