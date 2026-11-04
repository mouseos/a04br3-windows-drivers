/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "board_owner.h"
#include <assert.h>
#include <string.h>
struct fake { int leased,ok,quiet,badrail,badpin,badclock,lose_on_write,release_fail,drop_write;unsigned calls,writes,releases,pins;uint32_t top1,top2; };
static int acq(void *v,uint32_t g) { struct fake *f=v;assert(g==1);f->calls++;if(f->leased)return 0;f->leased=1;return 1; }
static int owned(void *v,uint32_t g) { struct fake *f=v;assert(g==1);f->calls++;return f->leased && f->ok; }
static int rails(void *v,struct a04_emmc_rails_raw *r) { struct fake *f=v;assert(f->leased);f->calls++;r->vemc_enable=f->badrail?0:0xc000;r->vemc_selector=0x80;r->vio18_enable=0xc000;return 0; }
static int pins(void *v,unsigned pin,unsigned *mode) { struct fake *f=v;assert(f->leased && pin==110+f->pins);f->pins++;f->calls++;*mode=(f->badpin && pin==115)?0:1;return 0; }
static int readclock(void *v,unsigned off,uint32_t *out) { struct fake *f=v;assert(f->leased);f->calls++;if(f->badclock)return -1;if(off==0x24)*out=f->top1;else { assert(off==0x3c);*out=f->top2; }return 0; }
static int writeclock(void *v,unsigned off,uint32_t value) { struct fake *f=v;assert(f->leased && f->ok);f->calls++;f->writes++;if(f->drop_write)return -1;if(off==0x84) {assert(value==(1u<<17));f->top1 &= ~value;} else {assert(off==0x6c && value==(1u<<28));f->top2|=value;}if(f->lose_on_write)f->ok=0;return 0; }
static int quiet(void *v) { struct fake *f=v;return f->quiet; }
static int release(void *v,uint32_t g) { struct fake *f=v;assert(g==1);f->releases++;if(f->release_fail)return 0;f->leased=0;return 1; }
static struct a04_board_ops ops(struct fake *f) { struct a04_board_ops o={f,acq,owned,rails,pins,readclock,writeclock,quiet,release};return o; }
int main(void)
{
    struct fake f={0};struct a04_board_owner s={0};struct a04_board_ops o=ops(&f);
    assert(a04_board_acquire_clocks(&s,&o,0)==A04_BOARD_INVALID && !f.calls);
    f.ok=1;f.quiet=1;assert(a04_board_begin(&s,&o,1)==A04_BOARD_OK && s.active && !s.prepared);
    assert(a04_board_release(&s,&o)==A04_BOARD_BUSY && f.leased && !f.releases);
    assert(a04_board_finish_clocks(&s,&o,1)==A04_BOARD_OK && s.prepared);
    assert(a04_board_finish_clocks(&s,&o,1)==A04_BOARD_BUSY);
    assert(a04_board_release(&s,&o)==A04_BOARD_OK && !f.leased && !s.prepared);
    memset(&f,0,sizeof f);memset(&s,0,sizeof s);
    f.ok=1;f.top1=1u<<17;assert(a04_board_acquire_clocks(&s,&o,1)==A04_BOARD_OK);
    assert(s.active && !s.poisoned && f.writes==2 && f.pins==11 && f.leased);
    assert(a04_board_acquire_clocks(&s,&o,1)==A04_BOARD_BUSY);
    assert(a04_board_release(&s,&o)==A04_BOARD_NOT_QUIESCENT && f.leased && !f.releases);
    f.quiet=1;assert(a04_board_release(&s,&o)==A04_BOARD_OK && !f.leased && !s.active);
    assert(a04_board_acquire_clocks(&s,&o,1)==A04_BOARD_BUSY);
    memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.ok=1;f.badrail=1;
    assert(a04_board_acquire_clocks(&s,&o,1)==A04_BOARD_RAILS && s.poisoned && s.active && !f.writes);
    assert(a04_board_release(&s,&o)==A04_BOARD_BUSY && f.leased);
    memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.ok=1;f.badpin=1;
    assert(a04_board_acquire_clocks(&s,&o,1)==A04_BOARD_PINS && s.poisoned && !f.writes);
    memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.ok=1;f.top1=1u<<17;f.lose_on_write=1;
    assert(a04_board_acquire_clocks(&s,&o,1)==A04_BOARD_OWNER_LOST && s.poisoned && f.writes==1);
    memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.ok=1;f.badclock=1;
    assert(a04_board_acquire_clocks(&s,&o,1)==A04_BOARD_CLOCK && s.poisoned && !f.writes);
    memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.ok=1;f.top1=1u<<17;f.drop_write=1;
    assert(a04_board_acquire_clocks(&s,&o,1)==A04_BOARD_CLOCK && s.poisoned && f.writes==1);
    memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.ok=1;f.top2=1u<<28;f.quiet=1;f.release_fail=1;
    assert(a04_board_acquire_clocks(&s,&o,1)==A04_BOARD_OK && !f.writes);
    assert(a04_board_release(&s,&o)==A04_BOARD_OWNER_LOST && s.poisoned && s.active);
    return 0;
}
