/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "charger.h"
#define NS 1000000000ULL
struct bus { uint64_t now; unsigned reads, writes; uint8_t regs[8]; };
static uint64_t now(void *p) { return ((struct bus *)p)->now; }
static int read_byte(void *p,uint8_t reg,uint8_t *v) { struct bus *b=p; ++b->reads; *v=b->regs[reg]; return 0; }
static int kick(void *p,uint8_t v) { struct bus *b=p; assert(v==0x80); ++b->writes; return 0; }
static void put(uint8_t *p,uint64_t v) { unsigned i; for(i=0;i<8;++i) p[i]=(uint8_t)(v>>(8*i)); }
static void seal(uint8_t *p) { uint64_t h=14695981039346656037ULL; unsigned i; for(i=0;i<120;i++){h^=p[i];h*=1099511628211ULL;} put(p+120,h); }
static void fixture(uint8_t p[128]) {
 const uint8_t bytes[8]={0x30,0,0x8c,0xa4,0x45,0x51,0,0x38};
 memset(p,0,128); put(p,0x314f524147484334ULL); put(p+8,128); put(p+16,1); put(p+24,130000000); put(p+32,13000000); put(p+40,143000000); put(p+48,2); put(p+56,1); put(p+64,255); memcpy(p+72,bytes,8); put(p+80,255); memcpy(p+88,bytes,8); seal(p);
}
int main(void) {
 uint8_t p[128], generated[128]; struct a04_bq_owner source; struct a04_bq_owner o; struct bus b; struct a04_bq_bus bus={&b,now,read_byte,kick}; unsigned off;
 assert(!a04_bq_check_counter_interval(100,13000100,13000000,NS,NS));
 assert(!a04_bq_check_counter_interval(100,101,13000000,76,77));
 assert(a04_bq_check_counter_interval(100,101,13000000,77,77));
 assert(a04_bq_check_counter_interval(100,100,13000000,1,NS));
 assert(a04_bq_check_counter_interval(101,100,13000000,1,NS));
 assert(a04_bq_check_counter_interval(100,13000100,13000000,2*NS,3*NS));
 assert(a04_bq_check_counter_interval(100,13000100,0,1,NS));
 assert(a04_bq_check_counter_interval(0,UINT64_MAX,1,1,UINT64_MAX));
 assert(a04_bq_check_counter_interval(0,1,13000000,0,NS));
 memset(&o,0,sizeof(o)); memset(&b,0,sizeof(b)); fixture(p); memcpy(b.regs,p+88,8); b.now=100*NS;
 assert(!a04_bq_adopt_handoff(&o,p,128,b.now,377000000,13000000));
 assert(o.handoff_pending && o.handoff_deadline==101*NS && o.firmware_kicks==2 && o.firmware_first.raw[0]==0x30);
 b.now=101*NS; assert(a04_bq_step(&o,&bus,200*NS)==A04_BQ_DEADLINE && !b.reads);
 b.now=100*NS; assert(a04_bq_step(&o,&bus,200*NS)==A04_BQ_STOPPED && !b.reads && !b.writes);
 assert(a04_bq_adopt_handoff(&o,p,128,b.now,143000000,13000000) && o.stopped);
 memset(&o,0,sizeof(o)); b.now=100*NS; assert(!a04_bq_adopt_handoff(&o,p,128,b.now,377000000,13000000));
 assert(a04_bq_step(&o,&bus,200*NS)==A04_BQ_OK && !o.handoff_pending && b.writes==1);
 source=o;
 assert(!a04_bq_publish_handoff(generated,128,&source,130000000,143000000,13000000));
 memset(&o,0,sizeof(o));
 assert(!a04_bq_adopt_handoff(&o,generated,128,100*NS,143000000,13000000));
 source.stopped=1; assert(a04_bq_publish_handoff(generated,128,&source,130000000,143000000,13000000));
 assert(generated[0]==0 && generated[127]==0);
 o=source; o.stopped=0; o.handoff_pending=0; o.kicks=1; o.last_kick=100*NS;
 b.now=120*NS; assert(a04_bq_step(&o,&bus,200*NS)==A04_BQ_DEADLINE);
 for(off=0;off<120;++off) { fixture(p); p[off]^=1; memset(&o,0,sizeof(o)); assert(a04_bq_adopt_handoff(&o,p,128,100*NS,377000000,13000000) && o.stopped); }
 fixture(p); memset(&o,0,sizeof(o)); assert(a04_bq_adopt_handoff(&o,p,128,100*NS,390000000,13000000));
 fixture(p); memset(&o,0,sizeof(o)); assert(a04_bq_adopt_handoff(&o,p,128,100*NS,142999999,13000000));
 fixture(p); memset(&o,0,sizeof(o)); assert(a04_bq_adopt_handoff(&o,p,128,UINT64_MAX,143000000,13000000));
 fixture(p); p[88]=0x31; seal(p); memset(&o,0,sizeof(o)); assert(a04_bq_adopt_handoff(&o,p,128,100*NS,143000000,13000000));
 fixture(p); p[56]=2; seal(p); memset(&o,0,sizeof(o)); assert(a04_bq_adopt_handoff(&o,p,128,100*NS,143000000,13000000));
 fixture(p); memset(&o,0,sizeof(o)); assert(!a04_bq_adopt_handoff(&o,p,128,100*NS,143000001,13000000));
 assert(o.handoff_deadline==119*NS-77); /* ceil(1/13MHz seconds) */
 assert(a04_bq_adopt_handoff(&o,p,128,100*NS,143000001,13000000) && o.stopped);
 return 0;
}
