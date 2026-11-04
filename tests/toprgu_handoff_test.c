/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "handoff_policy.h"
int main(void)
{
    unsigned cpus, cpu;
    for (cpus=0; cpus<=5; ++cpus) for (cpu=0; cpu<4; ++cpu) {
        assert(a04_toprgu_can_claim(cpus,0,cpu,1)==0);
        assert(a04_toprgu_can_claim(cpus,1,cpu,1)==(cpus==4 && cpu==1));
        assert(a04_toprgu_can_claim(cpus,1,cpu,2)==0);
        assert(a04_toprgu_can_feed(cpus,0)==0);
        assert(a04_toprgu_can_feed(cpus,1)==(cpus==4));
    }
    puts("CPU1 handoff, three CPU exclusion and CPU loss: OK");
    return 0;
}
