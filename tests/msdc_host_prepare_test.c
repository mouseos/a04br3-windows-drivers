/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "host_prepare.h"
#include <assert.h>
#include <string.h>
struct fake {uint32_t r[64];uint64_t ticks;unsigned writes,gates,owned_calls,lose_at;int gate_fail,stuck_reset,stuck_fifo,unstable,source_fail,lost;};
static int owned(void *p,uint32_t g){struct fake *f=p;assert(g==7);if(++f->owned_calls==f->lose_at)f->lost=1;return !f->lost;}
static int gate(void *p,uint32_t g,int off){struct fake *f=p;assert(g==7 && off==(f->gates%2==0));f->gates++;return !f->gate_fail;}
static int source(void *p,uint32_t g,uint32_t hz){struct fake *f=p;assert(g==7 && hz==188000000);return !f->source_fail;}
static uint64_t now(void *p){return ((struct fake *)p)->ticks++;}
static uint32_t rd(void *p,uint32_t off){struct fake *f=p;assert(!f->lost && off<sizeof(f->r));return f->r[off/4];}
static void wr(void *p,uint32_t off,uint32_t v){struct fake *f=p;assert(!f->lost && off<sizeof(f->r));f->writes++;
 if(off==0 && (v&4) && !f->stuck_reset)v&=~4u;
 if(off==0x14 && (v&0x80000000u) && !f->stuck_fifo)v&=~0x80000000u;
 if(off==0 && f->unstable)v&=~0x80u;
 if(off==0x0c)f->r[off/4]&=~v;else f->r[off/4]=v;}
int main(void){struct fake f={0};struct a04_msdc_io io={&f,rd,0,wr};struct a04_host_prepare_ops o={&io,&f,owned,gate,source,now,50,188000000};struct a04_host_prepare_state s={0};
 f.r[0]=0x80;f.r[0x10/4]=0xff;f.r[0x30/4]=3u<<16;
 assert(a04_host_prepare(&s,&o,7,20)==A04_HOST_OK && !s.poisoned && f.gates==2);
 assert((f.r[0] & (0xfffu<<8))==(181u<<8) && (f.r[0] & 9)==9 && !(f.r[0x30/4]&(3u<<16)) && !f.r[0x10/4]);
 memset(&f,0,sizeof f);f.source_fail=1;assert(a04_host_prepare(&s,&o,7,20)==A04_HOST_CLOCK && !f.writes && !s.poisoned);
 memset(&f,0,sizeof f);f.r[0x9c/4]=1;assert(a04_host_prepare(&s,&o,7,20)==A04_HOST_DMA_ACTIVE && !f.writes && !s.poisoned);
 memset(&f,0,sizeof f);f.stuck_reset=1;assert(a04_host_prepare(&s,&o,7,20)==A04_HOST_TIMEOUT && s.poisoned && f.writes==2);
 assert(a04_host_prepare(&s,&o,7,20)==A04_HOST_POISONED && f.writes==2);
 memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.gate_fail=1;assert(a04_host_prepare(&s,&o,7,20)==A04_HOST_CLOCK && s.poisoned && f.gates==1);
 memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.unstable=1;assert(a04_host_prepare(&s,&o,7,20)==A04_HOST_TIMEOUT && s.poisoned && f.gates==2);
 memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.lose_at=4;assert(a04_host_prepare(&s,&o,7,20)==A04_HOST_UNOWNED && s.poisoned && f.writes==1);
 /* Revoke at each ownership check in the complete power/clock sequence. */
 memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.r[0]=0x80;f.r[0x10/4]=0xff;f.r[0x30/4]=3u<<16;
 assert(a04_host_prepare(&s,&o,7,20)==A04_HOST_OK);
 {unsigned count=f.owned_calls,k;for(k=1;k<=count;k++) {
   memset(&f,0,sizeof f);memset(&s,0,sizeof s);f.r[0]=0x80;f.r[0x10/4]=0xff;f.r[0x30/4]=3u<<16;f.lose_at=k;
   assert(a04_host_prepare(&s,&o,7,20)!=A04_HOST_OK);
 }}
 memset(&f,0,sizeof f);memset(&s,0,sizeof s);o.source_hz_max=0xffffffffu;assert(a04_host_prepare(&s,&o,7,20)==A04_HOST_INVALID && !f.writes);
 return 0;}
