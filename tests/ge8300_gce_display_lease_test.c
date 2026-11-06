/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/gpu/mt8167/gce/gce_display_lease.h"
int main(void)
{
    struct a04_gce_display_lease s={0};uint32_t a=99,b=0;
    assert(!a04_gce_display_acquire(&s,&a) && !a);
    s.online=1;assert(a04_gce_display_acquire(&s,&a) && a==1);
    assert(!a04_gce_display_acquire(&s,&b) && !b);
    assert(!a04_gce_display_release(&s,a+1) && s.active==a);
    assert(a04_gce_display_is_live(&s,a));s.online=0;
    assert(!a04_gce_display_is_live(&s,a) && !a04_gce_display_acquire(&s,&b));
    assert(a04_gce_display_release(&s,a) && !s.active);
    s.online=1;assert(a04_gce_display_acquire(&s,&b) && b==2);
    s.removed=1;assert(!a04_gce_display_is_live(&s,b) && !a04_gce_display_acquire(&s,&a));
    assert(a04_gce_display_release(&s,b));s.removed=0;s.generation=UINT32_MAX;
    assert(!a04_gce_display_acquire(&s,&a) && !a);
    return 0;
}
