/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/gpu/mt8167/wddm-display/present_copy_fence.h"
int main(void)
{
    struct a04_ge8300_fenced_copy r={0};
    assert(!a04_present_copy_fence_valid(0,sizeof(r)));
    assert(!a04_present_copy_fence_valid(&r,sizeof(r)));
    r.version=1;r.task_sequence=5;r.flush_sequence=3;
    assert(a04_present_copy_fence_valid(&r,sizeof(r)));
    assert(!a04_present_copy_fence_valid(&r,sizeof(r)-1));
    r.task_sequence=0;assert(!a04_present_copy_fence_valid(&r,sizeof(r)));
    r.task_sequence=5;r.flush_sequence=0;assert(!a04_present_copy_fence_valid(&r,sizeof(r)));
    r.flush_sequence=3;r.version=2;assert(!a04_present_copy_fence_valid(&r,sizeof(r)));
    return 0;
}
