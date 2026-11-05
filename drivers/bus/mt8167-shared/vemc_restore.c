/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "vemc_restore.h"
#define INIT_DONE2 0x9cu
#define WACS_CMD 0xa0u
#define WACS_DATA 0xa4u
#define WACS_CLEAR 0xa8u
#define VEMC_CONTROL 0x0508u
#define VEMC_SELECTOR 0x052eu
#define VIO18_CONTROL 0x0556u
#define ENABLE (1u<<14)
#define QI (1u<<15)
struct op {struct a04_vemc_state *s;const struct a04_vemc_io *io;uint32_t gen;unsigned limit;uint64_t start;};
static enum a04_vemc_status fail(struct op *o,enum a04_vemc_status s){o->s->poisoned=1;return s;}
static enum a04_vemc_status check(struct op *o)
{
    uint64_t n;
    if(o->io->held_and_quiescent(o->io->context,o->gen)!=1)return fail(o,A04_VEMC_UNOWNED);
    n=o->io->now(o->io->context);
    if(n<o->start || n-o->start>=o->io->timeout_ticks)return fail(o,A04_VEMC_TIMEOUT);
    return A04_VEMC_OK;
}
static enum a04_vemc_status rd(struct op *o,unsigned off,uint32_t *v)
{ enum a04_vemc_status r=check(o);if(r)return r;return o->io->read32(o->io->context,off,v)?fail(o,A04_VEMC_IO):A04_VEMC_OK; }
static enum a04_vemc_status wr(struct op *o,unsigned off,uint32_t v)
{ enum a04_vemc_status r=check(o);if(r)return r;return o->io->write32(o->io->context,off,v)?fail(o,A04_VEMC_IO):A04_VEMC_OK; }
static enum a04_vemc_status fsm(struct op *o,unsigned target,uint32_t *last)
{
    unsigned i;enum a04_vemc_status r;
    for(i=0;i<o->limit;++i){r=rd(o,WACS_DATA,last);if(r)return r;if(((*last>>16)&7u)==target)return A04_VEMC_OK;}
    return fail(o,A04_VEMC_TIMEOUT);
}
static enum a04_vemc_status pmic_read(struct op *o,unsigned addr,uint16_t *out)
{
    uint32_t v;enum a04_vemc_status r;
    r=fsm(o,0,&v);if(r)return r;r=wr(o,WACS_CMD,(addr>>1)<<16);if(r)return r;
    r=fsm(o,6,&v);if(r)return r;*out=(uint16_t)v;
    r=wr(o,WACS_CLEAR,1);if(r)return r;return fsm(o,0,&v);
}
static enum a04_vemc_status pmic_enable(struct op *o,uint16_t value)
{
    uint32_t v;enum a04_vemc_status r=fsm(o,0,&v);if(r)return r;
    r=wr(o,WACS_CMD,0x80000000u|((VEMC_CONTROL>>1)<<16)|value);if(r)return r;
    return fsm(o,0,&v);
}
enum a04_vemc_status a04_vemc_restore(struct a04_vemc_state *s,const struct a04_vemc_io *io,
    uint32_t gen,unsigned selector,unsigned limit,struct a04_vemc_result *out)
{
    struct op o;struct a04_vemc_result v={0};uint32_t init;uint64_t before,after;enum a04_vemc_status r;
    if(!s||!io||!out||!io->held_and_quiescent||!io->read32||!io->write32||!io->wait_us||!io->now||
       !io->timeout_ticks||!io->ramp_ticks||io->ramp_ticks>=io->timeout_ticks||!gen||selector>1||!limit)return A04_VEMC_INVALID;
    if(s->poisoned)return A04_VEMC_POISONED;
    o.s=s;o.io=io;o.gen=gen;o.limit=limit;o.start=io->now(io->context);
    r=rd(&o,INIT_DONE2,&init);if(r)return r;if(!(init&1u))return fail(&o,A04_VEMC_UNSAFE);
    r=pmic_read(&o,VIO18_CONTROL,&v.vio18);if(r)return r;
    r=pmic_read(&o,VEMC_SELECTOR,&v.selector);if(r)return r;
    r=pmic_read(&o,VEMC_CONTROL,&v.control);if(r)return r;
    if((v.vio18&(ENABLE|QI))!=(ENABLE|QI) || ((v.selector>>7)&1u)!=selector)
        return fail(&o,A04_VEMC_UNSAFE);
    if((v.control&(ENABLE|QI))==QI || (v.control&(ENABLE|QI))==ENABLE)
        return fail(&o,A04_VEMC_UNSAFE);
    if(!(v.control&ENABLE)) {
        r=pmic_enable(&o,(uint16_t)(v.control|ENABLE));if(r)return r;v.wrote_enable=1;
        r=check(&o);if(r)return r;
        before=io->now(io->context);
        if(io->wait_us(io->context,264u))return fail(&o,A04_VEMC_IO);
        after=io->now(io->context);
        if(after<before || after-before<io->ramp_ticks)return fail(&o,A04_VEMC_UNSAFE);
        r=check(&o);if(r)return r;
        r=pmic_read(&o,VEMC_CONTROL,&v.control);if(r)return r;
        r=pmic_read(&o,VEMC_SELECTOR,&v.selector);if(r)return r;
        r=pmic_read(&o,VIO18_CONTROL,&v.vio18);if(r)return r;
        if((v.control&(ENABLE|QI))!=(ENABLE|QI) || (v.vio18&(ENABLE|QI))!=(ENABLE|QI) ||
           ((v.selector>>7)&1u)!=selector)return fail(&o,A04_VEMC_MISMATCH);
    }
    r=check(&o);if(r)return r;*out=v;return A04_VEMC_OK;
}
