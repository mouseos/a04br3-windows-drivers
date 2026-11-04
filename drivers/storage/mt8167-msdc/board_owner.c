/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "board_owner.h"
/* Register layout/polarity observed in the A04BR3 MT8167 Linux clock
 * provider; parent serializes all shared registers, including SET/CLR. */
#define TOP1_STATUS 0x24u
#define TOP1_CLEAR 0x84u
#define TOP2_STATUS 0x3cu
#define TOP2_SET 0x6cu
#define SOURCE (1u<<17)
#define INFRA (1u<<28)
#define ENABLE (1u<<14)
#define GOOD (1u<<15)
static int valid(const struct a04_board_ops *o)
{ return o && o->acquire && o->owned && o->read_rails && o->read_pin_mode && o->read_clock && o->write_clock && o->quiescent && o->release; }
static enum a04_board_result fail(struct a04_board_owner *s,enum a04_board_result r)
{ s->poisoned=1; return r; }
enum a04_board_result a04_board_acquire_clocks(struct a04_board_owner *s,const struct a04_board_ops *o,uint32_t generation)
{
    struct a04_emmc_rails_raw rails; unsigned pin,mode; uint32_t v;
    if(!s || !valid(o) || !generation) return A04_BOARD_INVALID;
    if(s->active || s->poisoned || generation<=s->generation) return A04_BOARD_BUSY;
    if(o->acquire(o->context,generation)!=1) return A04_BOARD_BUSY;
    s->active=1;s->generation=generation;
    if(o->owned(o->context,generation)!=1) return fail(s,A04_BOARD_OWNER_LOST);
    if(o->read_rails(o->context,&rails)!=0 ||
       (rails.vemc_enable&(ENABLE|GOOD))!=(ENABLE|GOOD) ||
       (rails.vio18_enable&(ENABLE|GOOD))!=(ENABLE|GOOD)) return fail(s,A04_BOARD_RAILS);
    for(pin=110;pin<=120;++pin) {
        if(o->owned(o->context,generation)!=1) return fail(s,A04_BOARD_OWNER_LOST);
        if(o->read_pin_mode(o->context,pin,&mode)!=0 || mode!=1) return fail(s,A04_BOARD_PINS);
    }
    if(o->owned(o->context,generation)!=1) return fail(s,A04_BOARD_OWNER_LOST);
    if(o->read_clock(o->context,TOP1_STATUS,&v)!=0) return fail(s,A04_BOARD_CLOCK);
    if(v&SOURCE) {
        if(o->owned(o->context,generation)!=1) return fail(s,A04_BOARD_OWNER_LOST);
        if(o->write_clock(o->context,TOP1_CLEAR,SOURCE)!=0) return fail(s,A04_BOARD_CLOCK);
    }
    if(o->owned(o->context,generation)!=1) return fail(s,A04_BOARD_OWNER_LOST);
    if(o->read_clock(o->context,TOP1_STATUS,&v)!=0 || (v&SOURCE)) return fail(s,A04_BOARD_CLOCK);
    if(o->read_clock(o->context,TOP2_STATUS,&v)!=0) return fail(s,A04_BOARD_CLOCK);
    if(!(v&INFRA)) {
        if(o->owned(o->context,generation)!=1) return fail(s,A04_BOARD_OWNER_LOST);
        if(o->write_clock(o->context,TOP2_SET,INFRA)!=0) return fail(s,A04_BOARD_CLOCK);
    }
    if(o->owned(o->context,generation)!=1) return fail(s,A04_BOARD_OWNER_LOST);
    if(o->read_clock(o->context,TOP2_STATUS,&v)!=0 || !(v&INFRA)) return fail(s,A04_BOARD_CLOCK);
    return A04_BOARD_OK;
}
enum a04_board_result a04_board_release(struct a04_board_owner *s,const struct a04_board_ops *o)
{
    if(!s || !valid(o)) return A04_BOARD_INVALID;
    if(!s->active || s->poisoned) return A04_BOARD_BUSY;
    if(o->owned(o->context,s->generation)!=1) return fail(s,A04_BOARD_OWNER_LOST);
    if(o->quiescent(o->context)!=1) return A04_BOARD_NOT_QUIESCENT;
    if(o->owned(o->context,s->generation)!=1) return fail(s,A04_BOARD_OWNER_LOST);
    if(o->release(o->context,s->generation)!=1) return fail(s,A04_BOARD_OWNER_LOST);
    s->active=0;return A04_BOARD_OK;
}
