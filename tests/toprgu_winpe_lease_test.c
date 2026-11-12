/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../drivers/system/mt8167-toprgu/winpe_lease.h"
int main(void)
{
    struct a04_winpe_lease s={0};
    const uint64_t span=UINT64_C(1800)*10000000u;
    assert(a04_winpe_lease_poll(&s,UINT64_MAX)==A04_WINPE_LEASE_CONTINUE);
    a04_winpe_lease_start(&s,UINT64_C(1)<<40);
    a04_winpe_lease_start(&s,(UINT64_C(1)<<40)+span-1); /* D0 reentry cannot renew */
    assert(s.started==(UINT64_C(1)<<40));
#if A04_TOPRGU_WINPE_LEASE_SECONDS
    assert(a04_winpe_lease_poll(&s,s.started-1)==A04_WINPE_LEASE_CONTINUE);
    assert(a04_winpe_lease_poll(&s,s.started+span-1)==A04_WINPE_LEASE_CONTINUE);
    assert(a04_winpe_lease_poll(&s,s.started+span)==A04_WINPE_LEASE_RESET);
    assert(a04_winpe_lease_poll(&s,s.started+span+1)==A04_WINPE_LEASE_HOLD);
    a04_winpe_lease_start(&s,0);
    assert(a04_winpe_lease_poll(&s,0)==A04_WINPE_LEASE_HOLD);
#else
    assert(a04_winpe_lease_poll(&s,UINT64_MAX)==A04_WINPE_LEASE_CONTINUE);
    assert(!s.expired);
#endif
    puts("WinPE lease: boundary, no-renew, one-reset, persistent feed-stop, default-off passed");
    return 0;
}
