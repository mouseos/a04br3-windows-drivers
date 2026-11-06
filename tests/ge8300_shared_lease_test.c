/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/gpu/mt8167/vendor_m1_11/shared_lease.h"
int main(void)
{
    struct a04_shared_lease_state s={0};uint32_t a,b;
    assert(!a04_shared_lease_acquire(&s));
    a04_shared_lease_copy_complete(&s,1,0);assert(!a04_shared_lease_acquire(&s));
    a04_shared_lease_copy_complete(&s,3,8);a=a04_shared_lease_acquire(&s);assert(a && s.task==3 && s.flush==8);
    assert(!a04_shared_lease_acquire(&s));assert(!a04_shared_lease_release(&s,a+1));
    assert(a04_shared_lease_release(&s,a));assert(!a04_shared_lease_release(&s,a));
    b=a04_shared_lease_acquire(&s);assert(b && a!=b);assert(a04_shared_lease_release(&s,b));
    s.next=UINT32_MAX;assert(!a04_shared_lease_acquire(&s));return 0;
}
