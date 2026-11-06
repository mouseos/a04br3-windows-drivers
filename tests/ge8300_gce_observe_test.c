/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include "../drivers/gpu/mt8167/gce/gce_observe.h"
struct fake {unsigned count,fail;};
static int readreg(void *p,uint32_t off,uint32_t *v)
{
    struct fake *f=p;f->count++;if(f->fail==f->count)return -1;*v=off;return 0;
}
int main(void)
{
    struct fake f={0};struct a04_gce_snapshot o={1,2,3,4};
    assert(a04_gce_observe_thread0(&f,0,readreg,&o) && !f.count && o.enable==1);
    assert(!a04_gce_observe_thread0(&f,1,readreg,&o) && f.count==4);
    assert(o.enable==0x104 && o.status==0x10c && o.irq_status==0x110 && o.irq_enable==0x114);
    f.count=0;f.fail=3;o.enable=7;o.status=8;o.irq_status=9;o.irq_enable=10;
    assert(a04_gce_observe_thread0(&f,1,readreg,&o) && f.count==3);
    assert(o.enable==7 && o.status==8 && o.irq_status==9 && o.irq_enable==10);
    return 0;
}
