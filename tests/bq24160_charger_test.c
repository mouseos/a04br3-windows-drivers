/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "charger.h"
struct fake { uint8_t r[8], written; uint64_t now, tick, kick_delay; int reads, writes, fail_read, fail_write, clocks, regress_at; };
static uint64_t clock_ns(void *p) { struct fake *f=p; uint64_t t=f->now; f->now+=f->tick; if (++f->clocks==f->regress_at) t-=f->tick*2; return t; }
static int read_reg(void *p, uint8_t reg, uint8_t *v) { struct fake *f=p; ++f->reads; if (f->fail_read == (int)reg) return -1; *v=f->r[reg]; return 0; }
static int kick(void *p, uint8_t v) { struct fake *f=p; ++f->writes; f->written=v; f->now += f->kick_delay; return f->fail_write; }
static void init(struct fake *f, struct a04_bq_owner *o, struct a04_bq_bus *b) {
    static const uint8_t regs[8]={0x30,0x00,0x8c,0xa4,0x45,0x51,0x00,0x38};
    memset(f,0,sizeof(*f)); memset(o,0,sizeof(*o)); memcpy(f->r,regs,8); f->fail_read=-1;
    b->context=f; b->now_ns=clock_ns; b->read=read_reg; b->write_watchdog=kick;
}
int main(void) {
    struct fake f; struct a04_bq_owner o; struct a04_bq_bus b; unsigned i;
    init(&f,&o,&b); assert(a04_bq_step(&o,&b,0)==A04_BQ_DEADLINE && !f.reads);
    init(&f,&o,&b); assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_OK);
    assert(f.reads==8 && f.writes==1 && f.written==0x80 && o.first.valid==255 && o.kicks==1);
    f.now=10000000000ULL; assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_OK);
    f.now=30000000000ULL; assert(a04_bq_step(&o,&b,40000000000ULL)==A04_BQ_DEADLINE && f.writes==2);
    assert(a04_bq_step(&o,&b,50000000000ULL)==A04_BQ_STOPPED);
    for (i=0;i<8;i++) { init(&f,&o,&b); f.fail_read=(int)i;
        assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_READ_ERROR);
        assert(o.first.valid==((1u<<i)-1u) && o.attempts==1 && !f.writes);
        assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_STOPPED); }
    init(&f,&o,&b); f.r[0]|=1; assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_FAULT && o.fault_history==2 && !f.writes);
    init(&f,&o,&b); f.r[2]^=1; assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_POLICY && !f.writes);
    init(&f,&o,&b); f.r[4]=0; assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_POLICY && !f.writes);
    init(&f,&o,&b); f.fail_write=1; assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_WRITE_ERROR && !o.kicks);
    init(&f,&o,&b); f.tick=1000000000ULL; assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_DEADLINE && !f.writes);
    init(&f,&o,&b); f.kick_delay=1000000000ULL; assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_DEADLINE && f.writes==1 && !o.kicks);
    init(&f,&o,&b); f.now=1000000000ULL; f.kick_delay=900000000ULL;
    assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_OK && o.last_kick==1000000000ULL);
    f.now=21000000000ULL;
    assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_DEADLINE && f.reads==8 && f.writes==1);
    init(&f,&o,&b); f.tick=100000000ULL;
    assert(a04_bq_step(&o,&b,500000000ULL)==A04_BQ_DEADLINE);
    assert(f.reads==4 && f.writes==0 && o.first.valid==15 && o.stopped);
    init(&f,&o,&b); f.tick=1000000000ULL;
    assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_DEADLINE);
    assert(f.reads==0 && o.attempts==0 && o.stopped);
    init(&f,&o,&b); f.now=100; f.tick=10; f.regress_at=4;
    assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_DEADLINE);
    assert(f.reads==2 && !f.writes && o.stopped);
    init(&f,&o,&b); o.kicks=1; o.last_kick=100; f.now=99; assert(a04_bq_step(&o,&b,30000000000ULL)==A04_BQ_DEADLINE && !f.reads);
    puts("BQ24160 policy and watchdog fake-bus tests passed"); return 0;
}
