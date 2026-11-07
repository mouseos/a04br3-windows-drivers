/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>
#include "d0_session.h"
#include "broker_lifecycle.h"
#include "cold_profile.h"
#define NS 1000000000ULL
struct fake { uint64_t now, gauge_delay; int reads,writes,gauge_reads,bad_gauge,bad_bq,reset_writes,reset_fail,bad_pec; uint8_t regs[8]; };
static uint64_t clock_ns(void *p) { return ((struct fake *)p)->now; }
static int read_bq(void *p,uint8_t reg,uint8_t *v) {
 struct fake *f=p;++f->reads;if(f->bad_bq)return -1;*v=f->regs[reg];return 0;
}
static int write_config(void *p,uint8_t reg,uint8_t v) {
 struct fake *f=p;assert(reg==7||reg==5||reg==3||reg==2);++f->writes;f->regs[reg]=v;return 0;
}
static int kick(void *p,uint8_t v) {struct fake *f=p;assert(v==0x80);++f->writes;return 0;}
static int reset_default(void *p) {
 struct fake *f=p;++f->reset_writes;
 /* Complete the bus write only; do not inspect hardware DEFAULT afterwards. */
 if(f->reset_fail)return -1;
 f->regs[2]=0x8c;f->regs[3]=0x5a;f->regs[5]=0;f->regs[7]=0;return 0;
}
static uint8_t crc(uint8_t c,uint8_t v) { unsigned i;c^=v;for(i=0;i<8;++i)c=(uint8_t)((c<<1)^((c&0x80)?7:0));return c; }
static int read_gauge(void *p,uint8_t a,uint8_t reg,uint8_t v[3]) {
    struct fake *f=p; uint16_t n=reg==0x15?1:reg==0x09?4000:500; uint8_t c=0;
    assert(a==0x0b); ++f->gauge_reads; f->now+=f->gauge_delay;
    if(f->bad_gauge)return -1;
    v[0]=(uint8_t)n;v[1]=(uint8_t)(n>>8);c=crc(c,0x16);c=crc(c,reg);c=crc(c,0x17);c=crc(c,v[0]);v[2]=crc(c,v[1])^(f->bad_pec ? 1u : 0u);return 0;
}

static void epoch(struct fake *f,struct a04_bq_d0_session *s,int expected,
                  struct a04_bq_owner *o,struct a04_bq_broker *b,
                  struct a04_broker_endpoint *e,struct a04_broker_lifecycle *l,
                  struct a04_bq_bus *c,struct a04_gauge_bus *g) {
 struct a04_bq_cold_profile_result profile;
 struct a04_bq_recovery_io config={f,clock_ns,read_bq,write_config};
 assert(a04_bq_d0_begin(s)==expected);
 memset(o,0,sizeof(*o));memset(b,0,sizeof(*b));memset(e,0,sizeof(*e));memset(l,0,sizeof(*l));
 c->context=f;c->now_ns=clock_ns;c->read=read_bq;c->write_watchdog=kick;
 g->context=f;g->read_word_pec=read_gauge;
 assert(!a04_bq_prepare_cold_diagnostic(o,f->now,f->now+10*NS));
 assert(!a04_bq_cold_profile(o,&config,&profile));
 assert(f->regs[2]==0x0c); /* USB100 only; no guessed USB classification. */
 assert(!a04_broker_lifecycle_start(l,e,b,o,c,g,f->now,UINT64_MAX,s->generation));
}
int main(void) {
 struct fake f={0};struct a04_bq_d0_session s={0};
 struct a04_bq_owner o;struct a04_bq_broker b;struct a04_broker_endpoint e;
 struct a04_broker_lifecycle l;struct a04_bq_bus c;struct a04_gauge_bus g;
 struct a04_gauge_sample sample;int result,reads;
 static const uint8_t initial[8]={0x30,0,0x8c,0x5a,0x45,0,0,0};
 memcpy(f.regs,initial,8);
 epoch(&f,&s,1,&o,&b,&e,&l,&c,&g);
 assert(!a04_broker_lifecycle_sample_local(&l,2*NS,&sample));
 assert(o.kicks==1&&sample.voltage_mv==4000&&sample.percent==47&&f.gauge_reads==3);
 assert(!a04_broker_lifecycle_quiesce(&l));reads=f.reads;
 result=a04_broker_lifecycle_default(&l,reset_default,&f);
 a04_bq_d0_default_result(&s,result);
 assert(!result&&s.generation==1&&a04_bq_d0_can_resume(&s)&&o.stopped&&e.closed);
 assert(f.reads==reads&&f.reset_writes==1); /* no BQ read after RESET */
 assert(a04_broker_lifecycle_sample_local(&l,2*NS,&sample)<0&&f.reads==reads);
 f.now=100*NS;epoch(&f,&s,2,&o,&b,&e,&l,&c,&g);
 assert(s.generation==2&&!a04_bq_d0_can_resume(&s));
 f.bad_pec=1;assert(a04_broker_lifecycle_sample_local(&l,f.now+2*NS,&sample)==1);
 assert(!o.stopped);f.bad_pec=0;
 assert(!a04_broker_lifecycle_sample_local(&l,f.now+2*NS,&sample));
 assert(!a04_broker_lifecycle_quiesce(&l));f.reset_fail=1;reads=f.reads;
 result=a04_broker_lifecycle_default(&l,reset_default,&f);
 a04_bq_d0_default_result(&s,result);
 assert(result&&!a04_bq_d0_can_resume(&s)&&!a04_bq_d0_begin(&s));
 assert(f.reads==reads&&f.reset_writes==2);
 /* No DEFAULT evidence, duplicate D0 entry, and overflow cannot open epochs. */
 memset(&s,0,sizeof(s));assert(a04_bq_d0_begin(&s)==1);assert(!a04_bq_d0_begin(&s));
 a04_bq_d0_default_result(&s,0);assert(!a04_bq_d0_can_resume(&s));
 memset(&s,0,sizeof(s));s.generation=UINT64_MAX;s.reset_complete=1;
 assert(!a04_bq_d0_begin(&s));
 /* Late owner cannot issue RESET or manufacture restart admission. */
 memset(&s,0,sizeof(s));memcpy(f.regs,initial,8);f.bad_bq=0;f.reset_fail=0;
 epoch(&f,&s,1,&o,&b,&e,&l,&c,&g);
 assert(!a04_broker_lifecycle_sample_local(&l,f.now+2*NS,&sample));
 assert(!a04_broker_lifecycle_quiesce(&l));f.now+=30*NS;reads=f.reset_writes;
 result=a04_broker_lifecycle_default(&l,reset_default,&f);
 a04_bq_d0_default_result(&s,result);assert(result&&f.reset_writes==reads&&!a04_bq_d0_begin(&s));
 return 0;
}
