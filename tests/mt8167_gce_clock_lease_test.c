/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/bus/mt8167-shared/gce_clock_lease.h"
struct fake {uint32_t state;unsigned reads,writes;int fail_write;};
static int rd(void *p,unsigned off,uint32_t *v){struct fake *f=p;assert(off==0x3c);f->reads++;*v=f->state;return 0;}
static int wr(void *p,unsigned off,uint32_t v){struct fake *f=p;assert(v==16);f->writes++;if(f->fail_write)return -1;if(off==0x9c)f->state&=~v;else {assert(off==0x6c);f->state|=v;}return 0;}
int main(void)
{
    struct fake f={16,0,0,0};struct a04_gce_clock_lease o={&f,rd,wr,0,0,1,0,0,0};uint32_t token=7;
    assert(a04_gce_clock_acquire(&o,&token) && !token && !f.reads && !f.writes);
    o.ownership_proven=1;assert(!a04_gce_clock_acquire(&o,&token) && token==1 && f.state==0 && f.writes==1);
    assert(a04_gce_clock_release(&o,token,0) && f.writes==1 && o.active==token);
    assert(a04_gce_clock_release(&o,token+1,1) && f.writes==1);
    assert(!a04_gce_clock_release(&o,token,1) && f.state==16 && f.writes==2);
    f.fail_write=1;assert(a04_gce_clock_acquire(&o,&token) && token==2 && o.poisoned && o.active==token);
    assert(a04_gce_clock_release(&o,token,1) && o.active==token);
    return 0;
}
