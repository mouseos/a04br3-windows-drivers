/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "owner_broker.h"
#include "broker_mailbox.h"
#include "broker_client.h"
#include "broker_wire.h"
#include "broker_handoff.h"
#include "broker_lifecycle.h"
#define NS 1000000000ULL
struct fake { uint64_t now, gauge_delay; int reads, writes, gauge_reads, bad_gauge, bad_pec, bad_bq, transfer_stage, transfer_fail, clock_calls, regress_at; };
static int reset_action(void *p){struct fake *f=p;++f->transfer_stage;return f->transfer_fail;}
static int transfer_action(void *p){struct fake *f=p;++f->transfer_stage;return f->transfer_stage==f->transfer_fail;}
struct loopback { struct fake *fake; struct a04_broker_endpoint *endpoint; };
static uint64_t loop_now(void *p) {return ((struct loopback *)p)->fake->now;}
static int loop_exchange(void *p,const struct a04_broker_request *q,struct a04_broker_response *r) {
 uint8_t wire[A04_BROKER_WIRE_BYTES];struct a04_broker_request received;struct a04_broker_response result;
 if(a04_broker_wire_request_encode(wire,sizeof(wire),q) || a04_broker_wire_request_decode(&received,wire,sizeof(wire)) ||
    a04_broker_endpoint_dispatch(((struct loopback *)p)->endpoint,&received,&result) ||
    a04_broker_wire_response_encode(wire,sizeof(wire),&result) || a04_broker_wire_response_decode(r,wire,sizeof(wire))) return -1;
 return 0;
}
static uint64_t clock_ns(void *p) { struct fake *f=p;++f->clock_calls;return f->clock_calls==f->regress_at?f->now-1:f->now; }
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
    v[0]=(uint8_t)n;v[1]=(uint8_t)(n>>8);c=crc(c,0x16);c=crc(c,reg);c=crc(c,0x17);c=crc(c,v[0]);v[2]=(uint8_t)(crc(c,v[1]) ^ (f->bad_pec ? 1 : 0));return 0;
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
    /* PEC failure is bad data, not a transport fault: retain ownership,
     * invalidate the sample, and continue scheduled watchdog maintenance. */
    setup(&f,&o,&b,&c,&g);old=out;f.bad_pec=1;
    assert(a04_bq_broker_run(&b,1,5*NS,&out)==A04_BROKER_REJECTED);
    assert(!b.stopped&&!o.stopped&&!memcmp(&old,&out,sizeof(out))&&f.writes==1);
    f.now=10*NS;assert(a04_bq_broker_run(&b,0,0,0)==A04_BROKER_OK&&f.writes==2);
    f.bad_pec=0;assert(a04_bq_broker_run(&b,2,15*NS,&out)==A04_BROKER_OK);
    setup(&f,&o,&b,&c,&g);f.bad_pec=1;f.gauge_delay=NS;
    assert(a04_bq_broker_run(&b,1,5*NS,&out)==A04_BROKER_STOPPED&&o.stopped);
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
        setup(&f,&o,&b,&c,&g);
        memset(&r,0xaa,sizeof(r));assert(a04_broker_endpoint_dispatch(&e,&q,&r));
        assert(!r.epoch&&!f.reads&&!f.writes&&!f.gauge_reads);
        assert(a04_broker_endpoint_maintain(&e)&&!f.reads&&!f.writes);
        assert(!a04_broker_endpoint_init(&e,&b,7));
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
    {
        struct a04_broker_endpoint e={0};struct a04_broker_request q={1,1,25*NS};
        struct a04_broker_response r;
        setup(&f,&o,&b,&c,&g);assert(a04_broker_endpoint_quiesce(&e));
        assert(!a04_broker_endpoint_init(&e,&b,1));
        assert(!a04_broker_endpoint_maintain(&e)&&f.writes==1);
        assert(!a04_broker_endpoint_quiesce(&e)&&e.quiescing);
        assert(a04_broker_endpoint_quiesce(&e));
        assert(a04_broker_endpoint_dispatch(&e,&q,&r)&&!f.gauge_reads);
        f.now=10*NS;assert(!a04_broker_endpoint_maintain(&e)&&f.writes==2);
        f.now=30*NS;assert(a04_broker_endpoint_maintain(&e)&&o.stopped&&e.closed);
        assert(a04_broker_endpoint_maintain(&e)&&f.writes==2);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_broker_client client={0};
        struct loopback l={&f,&e};struct a04_client_transport t={&l,loop_now,loop_exchange};
        setup(&f,&o,&b,&c,&g);assert(!a04_broker_endpoint_init(&e,&b,8));
        assert(!a04_broker_client_init(&client,&t,8,0));
        assert(!a04_broker_client_read(&client,5*NS,&out));
        assert(f.writes==1&&f.gauge_reads==3&&out.voltage_mv==4000);
        f.bad_gauge=1;assert(a04_broker_client_read(&client,5*NS,&out));
        assert(e.closed&&client.stopped&&o.stopped);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_owner_transfer tr={0};struct a04_bq_owner destination={0};
        struct a04_transfer_ops ops={&f,transfer_action,transfer_action,transfer_action,transfer_action,clock_ns};
        setup(&f,&o,&b,&c,&g);assert(!a04_broker_endpoint_init(&e,&b,1));
        assert(!a04_broker_endpoint_maintain(&e)&&!a04_broker_endpoint_quiesce(&e));f.now=2*NS;
        assert(!a04_broker_handoff(&e,&destination,&tr,&ops,30*NS));
        assert(e.closed&&b.stopped&&o.stopped&&tr.state==A04_TRANSFER_NEW&&f.transfer_stage==4);
        assert(destination.handoff_pending&&destination.handoff_deadline==10*NS&&destination.kicks==1);
        f.now=10*NS;assert(a04_bq_step(&destination,&c,30*NS)==A04_BQ_DEADLINE&&f.writes==1);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_owner_transfer tr={0};struct a04_bq_owner destination={0};
        struct a04_transfer_ops ops={&f,transfer_action,transfer_action,transfer_action,transfer_action,clock_ns};
        setup(&f,&o,&b,&c,&g);assert(!a04_broker_endpoint_init(&e,&b,1));
        assert(!a04_broker_endpoint_maintain(&e)&&!a04_broker_endpoint_quiesce(&e));f.now=2*NS;
        assert(!a04_broker_handoff(&e,&destination,&tr,&ops,30*NS));
        f.now=9*NS;assert(a04_bq_step(&destination,&c,30*NS)==A04_BQ_OK);
        assert(!destination.handoff_pending&&destination.kicks==2&&f.writes==2&&o.stopped);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_owner_transfer tr={0};struct a04_bq_owner destination={0};
        struct a04_transfer_ops ops={&f,transfer_action,transfer_action,transfer_action,transfer_action,clock_ns};
        setup(&f,&o,&b,&c,&g);assert(!a04_broker_endpoint_init(&e,&b,1));
        assert(!a04_broker_endpoint_maintain(&e)&&!a04_broker_endpoint_quiesce(&e));f.transfer_fail=3;
        assert(a04_broker_handoff(&e,&destination,&tr,&ops,30*NS));
        assert(tr.state==A04_TRANSFER_FAILED&&e.closed&&o.stopped&&destination.stopped&&f.transfer_stage==3);
        assert(a04_bq_step(&destination,&c,30*NS)==A04_BQ_STOPPED&&f.writes==1);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_owner_transfer tr={0};struct a04_bq_owner destination={0};
        struct a04_transfer_ops ops={&f,transfer_action,transfer_action,transfer_action,transfer_action,clock_ns};
        setup(&f,&o,&b,&c,&g);assert(!a04_broker_endpoint_init(&e,&b,1));
        assert(!a04_broker_endpoint_maintain(&e)&&!a04_broker_endpoint_quiesce(&e));f.now=10*NS;
        assert(a04_broker_handoff(&e,&destination,&tr,&ops,30*NS));
        assert(!tr.attempted&&!destination.stopped&&!f.transfer_stage&&!e.closed);
        assert(a04_broker_endpoint_maintain(&e)==0&&f.writes==2);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_owner_transfer tr={0};struct a04_bq_owner destination={0};
        struct a04_transfer_ops ops={&f,transfer_action,transfer_action,transfer_action,transfer_action,clock_ns};
        setup(&f,&o,&b,&c,&g);assert(!a04_broker_endpoint_init(&e,&b,1));
        assert(!a04_broker_endpoint_maintain(&e)&&!a04_broker_endpoint_quiesce(&e));
        f.now=2*NS;f.clock_calls=0;f.regress_at=7;
        assert(a04_broker_handoff(&e,&destination,&tr,&ops,30*NS));
        assert(tr.state==A04_TRANSFER_FAILED&&e.closed&&o.stopped&&destination.stopped);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_owner_transfer tr={0};struct a04_bq_owner destination={0};
        struct a04_transfer_ops ops={&f,transfer_action,transfer_action,transfer_action,transfer_action,clock_ns};
        setup(&f,&o,&b,&c,&g);assert(!a04_broker_endpoint_init(&e,&b,1));
        assert(!a04_broker_endpoint_maintain(&e)&&!a04_broker_endpoint_quiesce(&e));
        f.now=2*NS;f.clock_calls=0;f.regress_at=2;
        assert(a04_broker_handoff(&e,&destination,&tr,&ops,30*NS));
        assert(tr.state==A04_TRANSFER_FAILED&&f.transfer_stage==0&&e.closed&&destination.stopped);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_owner_transfer tr={0};struct a04_bq_owner destination={0};
        struct a04_transfer_ops ops={&f,transfer_action,transfer_action,transfer_action,transfer_action,clock_ns};
        setup(&f,&o,&b,&c,&g);assert(!a04_broker_endpoint_init(&e,&b,1));
        assert(!a04_broker_endpoint_maintain(&e)&&!a04_broker_endpoint_quiesce(&e));
        f.now=2*NS;tr.last_observed=3*NS;
        assert(a04_broker_handoff(&e,&destination,&tr,&ops,30*NS));
        assert(!tr.attempted&&!f.transfer_stage&&!e.closed&&!o.stopped);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_broker_lifecycle l={0};struct a04_owner_transfer tr={0};
        struct a04_bq_owner destination={0};struct a04_broker_request q={1,1,5*NS};struct a04_broker_response response;
        struct a04_transfer_ops ops={&f,transfer_action,transfer_action,transfer_action,transfer_action,clock_ns};
        setup(&f,&o,&b,&c,&g);memset(&b,0,sizeof(b));
        assert(!a04_broker_lifecycle_start(&l,&e,&b,&o,&c,&g,0,40*NS,1));
        assert(!f.reads&&!f.writes&&l.state==A04_LIFE_LIVE);
        assert(!a04_broker_lifecycle_maintain(&l)&&f.writes==1);
        assert(!a04_broker_lifecycle_sample_local(&l,5*NS,&out)&&out.voltage_mv==4000&&!b.last_request);
        assert(!a04_broker_endpoint_dispatch(&e,&q,&response)&&response.result==A04_BROKER_OK);
        assert(b.last_request==1);
        assert(!a04_broker_lifecycle_quiesce(&l));
        assert(a04_broker_endpoint_dispatch(&e,&q,&response));
        assert(a04_broker_lifecycle_sample_local(&l,5*NS,&out)&&!l.endpoint->closed);
        f.now=2*NS;assert(!a04_broker_lifecycle_handoff(&l,&destination,&tr,&ops,30*NS));
        assert(l.state==A04_LIFE_HANDED_OFF&&destination.handoff_pending&&o.stopped);
        a04_broker_lifecycle_stop(&l);assert(!destination.stopped);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_broker_lifecycle l={0};
        setup(&f,&o,&b,&c,&g);memset(&b,0,sizeof(b));
        assert(!a04_broker_lifecycle_start(&l,&e,&b,&o,&c,&g,0,40*NS,1));
        assert(!a04_broker_lifecycle_maintain(&l)&&!a04_broker_lifecycle_quiesce(&l));
        a04_broker_lifecycle_stop(&l);
        assert(l.state==A04_LIFE_STOPPED&&e.closed&&o.stopped);
        assert(a04_broker_lifecycle_maintain(&l)&&a04_broker_lifecycle_quiesce(&l));
        assert(a04_broker_lifecycle_start(&l,&e,&b,&o,&c,&g,0,40*NS,1));
    }
    {
        struct a04_broker_endpoint e={0};struct a04_broker_lifecycle l={0};
        setup(&f,&o,&b,&c,&g);memset(&b,0,sizeof(b));
        assert(!a04_broker_lifecycle_start(&l,&e,&b,&o,&c,&g,0,40*NS,1));
        f.bad_gauge=1;old=out;
        assert(a04_broker_lifecycle_sample_local(&l,5*NS,&out));
        assert(l.state==A04_LIFE_STOPPED&&e.closed&&o.stopped&&!memcmp(&out,&old,sizeof(out)));
    }
    {
        struct a04_broker_endpoint e={0};struct a04_broker_lifecycle l={0};
        setup(&f,&o,&b,&c,&g);memset(&b,0,sizeof(b));
        assert(!a04_broker_lifecycle_start(&l,&e,&b,&o,&c,&g,0,40*NS,1));
        f.bad_pec=1;old=out;
        assert(a04_broker_lifecycle_sample_local(&l,5*NS,&out)==1);
        assert(l.state==A04_LIFE_LIVE&&!e.closed&&!o.stopped&&!memcmp(&out,&old,sizeof(out)));
        f.now=10*NS;assert(!a04_broker_lifecycle_maintain(&l)&&f.writes==2);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_broker_lifecycle l={0};
        setup(&f,&o,&b,&c,&g);memset(&b,0,sizeof(b));
        assert(!a04_broker_lifecycle_start(&l,&e,&b,&o,&c,&g,0,40*NS,1));
        assert(!a04_broker_lifecycle_maintain(&l));
        assert(!a04_broker_lifecycle_quiesce(&l));f.now=2*NS;
        assert(!a04_broker_lifecycle_default(&l,reset_action,&f));
        assert(f.transfer_stage==1 && l.state==A04_LIFE_STOPPED && e.closed && o.stopped);
        assert(a04_broker_lifecycle_default(&l,reset_action,&f) && f.transfer_stage==1);
        assert(a04_broker_lifecycle_maintain(&l) && f.writes==1);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_broker_lifecycle l={0};
        setup(&f,&o,&b,&c,&g);memset(&b,0,sizeof(b));
        assert(!a04_broker_lifecycle_start(&l,&e,&b,&o,&c,&g,0,40*NS,1));
        assert(!a04_broker_lifecycle_maintain(&l));assert(!a04_broker_lifecycle_quiesce(&l));
        f.now=10*NS;assert(a04_broker_lifecycle_default(&l,reset_action,&f));
        assert(!f.transfer_stage && e.closed && o.stopped);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_broker_lifecycle l={0};
        setup(&f,&o,&b,&c,&g);memset(&b,0,sizeof(b));
        assert(!a04_broker_lifecycle_start(&l,&e,&b,&o,&c,&g,0,40*NS,1));
        assert(!a04_broker_lifecycle_maintain(&l));assert(!a04_broker_lifecycle_quiesce(&l));
        f.transfer_fail=1;assert(a04_broker_lifecycle_default(&l,reset_action,&f));
        assert(f.transfer_stage==1 && e.closed && o.stopped);
        assert(a04_broker_lifecycle_default(&l,reset_action,&f) && f.transfer_stage==1);
    }
    {
        struct a04_broker_endpoint e={0};struct a04_broker_lifecycle l={0};
        setup(&f,&o,&b,&c,&g);memset(&b,0,sizeof(b));o.handoff_deadline=5*NS;f.now=5*NS;
        assert(a04_broker_lifecycle_start(&l,&e,&b,&o,&c,&g,f.now,40*NS,1));
        assert(l.state==A04_LIFE_STOPPED&&o.stopped&&e.closed&&!f.reads&&!f.writes);
    }
    {
        uint8_t wire[A04_BROKER_WIRE_BYTES];struct a04_broker_request q={1,2,3},decoded={9,9,9};
        struct a04_broker_response r={0},response={0};unsigned i;
        assert(!a04_broker_wire_request_encode(wire,sizeof(wire),&q));
        assert(!a04_broker_wire_request_decode(&decoded,wire,sizeof(wire))&&decoded.deadline==3);
        for(i=32;i<sizeof(wire);++i){wire[i]=1;assert(a04_broker_wire_request_decode(&decoded,wire,sizeof(wire)));wire[i]=0;}
        assert(a04_broker_wire_request_decode(&decoded,wire,sizeof(wire)-1));
        r.epoch=1;r.sequence=2;r.result=A04_BROKER_STOPPED;
        assert(!a04_broker_wire_response_encode(wire,sizeof(wire),&r));
        assert(!a04_broker_wire_response_decode(&response,wire,sizeof(wire))&&response.result==A04_BROKER_STOPPED);
        wire[25]=1;assert(a04_broker_wire_response_decode(&response,wire,sizeof(wire)));
        wire[25]=0;wire[30]=1;assert(a04_broker_wire_response_decode(&response,wire,sizeof(wire)));
        r.sample.percent=1;assert(a04_broker_wire_response_encode(wire,sizeof(wire),&r));
    }
    return 0;
}
