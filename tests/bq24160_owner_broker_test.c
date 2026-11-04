/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "owner_broker.h"
#include "broker_mailbox.h"
#define NS 1000000000ULL
struct fake { uint64_t now, gauge_delay; int reads, writes, gauge_reads, bad_gauge, bad_bq; };
static uint64_t clock_ns(void *p) { return ((struct fake *)p)->now; }
static int read_bq(void *p,uint8_t r,uint8_t *v) {
    static const uint8_t data[8]={0x30,0,0x8c,0xa4,0x45,0x51,0,0x38};
    struct fake *f=p; ++f->reads; if(f->bad_bq)return -1; *v=data[r]; return 0;
}
static int write_bq(void *p,uint8_t v) { struct fake *f=p; assert(v==0x80); ++f->writes; return 0; }
static uint8_t crc(uint8_t c,uint8_t v) { unsigned i;c^=v;for(i=0;i<8;++i)c=(uint8_t)((c<<1)^((c&0x80)?7:0));return c; }
static int read_gauge(void *p,uint8_t a,uint8_t reg,uint8_t v[3]) {
    struct fake *f=p; uint16_t n=reg==0x15?1:reg==0x09?4000:500; uint8_t c=0;
    assert(a==0x0b); ++f->gauge_reads; f->now+=f->gauge_delay;
    if(f->bad_gauge)return -1;
    v[0]=(uint8_t)n;v[1]=(uint8_t)(n>>8);c=crc(c,0x16);c=crc(c,reg);c=crc(c,0x17);c=crc(c,v[0]);v[2]=crc(c,v[1]);return 0;
}
static void setup(struct fake *f,struct a04_bq_owner *o,struct a04_bq_broker *b,
                  struct a04_bq_bus *c,struct a04_gauge_bus *g) {
    memset(f,0,sizeof(*f));memset(o,0,sizeof(*o));memset(b,0,sizeof(*b));
    o->firmware_kicks=1;o->handoff_pending=1;o->handoff_deadline=20*NS;
    c->context=f;c->now_ns=clock_ns;c->read=read_bq;c->write_watchdog=write_bq;
    g->context=f;g->read_word_pec=read_gauge;
    assert(!a04_bq_broker_init(b,o,c,g,0,40*NS));
}
int main(void) {
    struct fake f;struct a04_bq_owner o;struct a04_bq_broker b;struct a04_bq_bus c;
    struct a04_gauge_bus g;struct a04_gauge_sample out,old;
    setup(&f,&o,&b,&c,&g);memset(&out,0,sizeof(out));
    assert(a04_bq_broker_run(&b,1,5*NS,&out)==A04_BROKER_OK);
    assert(f.writes==1&&f.reads==8&&f.gauge_reads==3&&out.voltage_mv==4000);
    old=out;assert(a04_bq_broker_run(&b,1,5*NS,&out)==A04_BROKER_REJECTED);
    assert(f.gauge_reads==3&&!memcmp(&old,&out,sizeof(out)));
    f.now=10*NS;assert(a04_bq_broker_run(&b,0,0,0)==A04_BROKER_OK&&f.writes==2);
    f.bad_gauge=1;assert(a04_bq_broker_run(&b,2,15*NS,&out)==A04_BROKER_STOPPED);
    assert(o.stopped&&!memcmp(&old,&out,sizeof(out)));
    f.bad_gauge=0;assert(a04_bq_broker_run(&b,3,15*NS,&out)==A04_BROKER_STOPPED&&f.writes==2);
    setup(&f,&o,&b,&c,&g);f.bad_bq=1;
    assert(a04_bq_broker_run(&b,1,5*NS,&out)==A04_BROKER_STOPPED&&f.gauge_reads==0);
    setup(&f,&o,&b,&c,&g);f.gauge_delay=400000000;
    assert(a04_bq_broker_run(&b,1,5*NS,&out)==A04_BROKER_STOPPED&&o.stopped);
    setup(&f,&o,&b,&c,&g);f.gauge_delay=100000000;old=out;
    assert(a04_bq_broker_run(&b,1,200000000,&out)==A04_BROKER_REJECTED);
    assert(!b.stopped&&!memcmp(&old,&out,sizeof(out)));
    f.now=9500000000ULL;assert(a04_bq_broker_run(&b,2,12*NS,&out)==A04_BROKER_OK&&f.writes==2);
    setup(&f,&o,&b,&c,&g);f.now=40*NS;
    assert(a04_bq_broker_run(&b,0,0,0)==A04_BROKER_STOPPED&&!f.reads);
    setup(&f,&o,&b,&c,&g);f.now=NS;assert(a04_bq_broker_run(&b,0,0,0)==A04_BROKER_OK);
    f.now=0;assert(a04_bq_broker_run(&b,0,0,0)==A04_BROKER_STOPPED&&o.stopped);
    setup(&f,&o,&b,&c,&g);a04_bq_broker_stop(&b);
    assert(a04_bq_broker_init(&b,&o,&c,&g,0,40*NS));
    {
        struct a04_broker_endpoint e={0};
        struct a04_broker_request q={7,1,5*NS};
        struct a04_broker_response r;
        setup(&f,&o,&b,&c,&g);assert(!a04_broker_endpoint_init(&e,&b,7));
        q.epoch=8;memset(&r,0xaa,sizeof(r));
        assert(a04_broker_endpoint_dispatch(&e,&q,&r)&&!f.reads&&!r.epoch);
        q.epoch=7;assert(!a04_broker_endpoint_dispatch(&e,&q,&r));
        assert(r.epoch==7&&r.sequence==1&&r.result==A04_BROKER_OK&&r.sample.voltage_mv==4000);
        assert(a04_broker_endpoint_dispatch(&e,&q,&r)&&!r.epoch&&f.gauge_reads==3);
        q.sequence=2;f.bad_gauge=1;
        assert(!a04_broker_endpoint_dispatch(&e,&q,&r)&&r.result==A04_BROKER_STOPPED);
        assert(e.closed&&o.stopped&&!r.sample.voltage_mv);
        q.sequence=3;assert(a04_broker_endpoint_dispatch(&e,&q,&r)&&!r.epoch);
        assert(a04_broker_endpoint_init(&e,&b,9));
    }
    {
        struct a04_broker_endpoint e={0};struct a04_broker_request q={1,1,5*NS};
        struct a04_broker_response r;
        setup(&f,&o,&b,&c,&g);assert(!a04_broker_endpoint_init(&e,&b,1));
        assert(!a04_broker_endpoint_maintain(&e)&&f.writes==1);
        a04_broker_endpoint_close(&e);
        assert(o.stopped&&a04_broker_endpoint_dispatch(&e,&q,&r)&&f.reads==8);
        assert(a04_broker_endpoint_maintain(&e)&&f.writes==1);
    }
    return 0;
}
