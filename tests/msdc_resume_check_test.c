/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "resume_check.h"
static struct a04_resume_observation good(unsigned g)
{
    struct a04_resume_observation x={0};
    x.generation=g;x.sectors=12345;x.rca=1;x.cid[0]=1;
    x.card_status=(4u<<9)|(1u<<8);x.host_width=x.card_width=8;
    x.power_owned=x.clock_stable=x.pio_mode=x.dma_idle=x.events_masked=1;
    return x;
}
int main(void)
{
    struct a04_resume_observation b=good(1),c=good(2);
    assert(a04_resume_validate(&b,&c)==A04_RESUME_VALID);
    assert(a04_resume_validate(0,&c)==A04_RESUME_INVALID);
    c.generation=1;assert(a04_resume_validate(&b,&c)==A04_RESUME_GENERATION);c=good(2);
    c.power_owned=2;assert(a04_resume_validate(&b,&c)==A04_RESUME_OWNER);c=good(2);
    c.dma_idle=0;assert(a04_resume_validate(&b,&c)==A04_RESUME_OWNER);c=good(2);
    c.card_status=3u<<9;assert(a04_resume_validate(&b,&c)==A04_RESUME_CARD_STATE);c=good(2);
    c.card_status|=1u<<31;assert(a04_resume_validate(&b,&c)==A04_RESUME_CARD_STATE);c=good(2);
    c.card_status|=1u<<7;assert(a04_resume_validate(&b,&c)==A04_RESUME_CARD_STATE);c=good(2);
    c.cid[5]=3;assert(a04_resume_validate(&b,&c)==A04_RESUME_IDENTITY);c=good(2);
    c.sectors++;assert(a04_resume_validate(&b,&c)==A04_RESUME_IDENTITY);c=good(2);
    c.partition=1;assert(a04_resume_validate(&b,&c)==A04_RESUME_MODE);c=good(2);
    c.cache_ctrl=1;assert(a04_resume_validate(&b,&c)==A04_RESUME_MODE);c=good(2);
    c.card_width=4;assert(a04_resume_validate(&b,&c)==A04_RESUME_MODE);c=good(2);
    b.cid[0]=c.cid[0]=0;assert(a04_resume_validate(&b,&c)==A04_RESUME_IDENTITY);
    return 0;
}
