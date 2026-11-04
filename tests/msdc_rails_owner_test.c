/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "pwrap_owner.h"
#include <assert.h>
#include <string.h>
struct fake { int locked,online,fsm,fail_index,txn; unsigned writes; uint32_t address; };
static int lock(void *p) { struct fake *f=p;assert(!f->locked);f->locked=1;return 0; }
static void unlock(void *p) { struct fake *f=p;assert(f->locked);f->locked=0; }
static uint32_t rd(void *p,unsigned off)
{
    struct fake *f=p;assert(f->locked);if(off==0x9c)return 1;assert(off==0xa4);
    if(f->fail_index && f->txn==f->fail_index && f->fsm==6)return 0;
    if(f->fsm==6) return (6u<<16)|(f->address==0x0508?0xc001u:f->address==0x052e?0x80u:0xc002u);
    return 0;
}
static void wr(void *p,unsigned off,uint32_t val)
{
    struct fake *f=p;assert(f->locked);++f->writes;
    if(off==0xa0) { assert(!(val&0x80000000u)); f->address=((val>>16)&0x7fffu)*2u;assert(f->address==0x0508 || f->address==0x052e || f->address==0x0556);f->txn++;f->fsm=6; }
    else { assert(off==0xa8 && val==1);f->fsm=0; }
}
static void delay(void *p) { (void)p; }
int main(void)
{
    struct fake f={0};struct a04_pwrap_owner o={0};struct a04_emmc_rails_raw x={1,2,3},old=x;
    o.lock_context=&f;o.lock=lock;o.unlock=unlock;o.bus.context=&f;o.bus.read32=rd;o.bus.write32=wr;o.bus.delay=delay;o.bus.attempts=3;
    assert(a04_pwrap_owner_read_emmc_rails(&o,&x)!=0 && !memcmp(&x,&old,sizeof x) && !f.writes);
    assert(!a04_pwrap_owner_start(&o));assert(!a04_pwrap_owner_read_emmc_rails(&o,&x));
    assert(x.vemc_enable==0xc001 && x.vemc_selector==0x80 && x.vio18_enable==0xc002 && f.txn==3 && !f.locked);
    assert(!a04_pwrap_owner_stop(&o));old=x;assert(a04_pwrap_owner_read_emmc_rails(&o,&x)!=0 && !memcmp(&x,&old,sizeof x));
    memset(&f,0,sizeof f);o.online=0;o.bus.poisoned=0;f.fail_index=2;assert(!a04_pwrap_owner_start(&o));
    assert(a04_pwrap_owner_read_emmc_rails(&o,&x)!=0 && !memcmp(&x,&old,sizeof x) && o.bus.poisoned && !f.locked);
    { unsigned w=f.writes;assert(a04_pwrap_owner_read_emmc_rails(&o,&x)!=0 && f.writes==w); }
    return 0;
}
