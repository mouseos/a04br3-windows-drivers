/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <string.h>
#include "stop.h"
struct bus {unsigned ops,fail,busy,drain_fail,lost;uint32_t mask,pause,periodic;};
static int qual(void *p){return !((struct bus *)p)->lost;}
static int rd(void *p,uint32_t a,uint32_t *v){struct bus *b=p;if(++b->ops==b->fail)return -1;switch(a-0x1100d000u){case 0xc:*v=b->mask;break;case 0x3c:*v=b->pause;break;case 0:*v=b->periodic;break;case 0x418:*v=b->busy?0x10000u:0;break;default:assert(0);}return 0;}
static int wr(void *p,uint32_t a,uint32_t v){struct bus *b=p;switch(a-0x1100d000u){case 0xc:b->mask=v;break;case 0x3c:b->pause=v;break;case 0:b->periodic=v;break;default:assert(0);}return ++b->ops==b->fail?-1:0;}
static int delay(void *p,unsigned us){struct bus *b=p;assert(us==2);return ++b->ops==b->fail?-1:0;}
static int drain(void *p){struct bus *b=p;assert(!b->mask);return ++b->ops==b->fail||b->drain_fail?-1:0;}
static int run(struct bus *b,struct a04_thermal_stop_state *s){struct a04_thermal_hw hw={0};struct a04_thermal_stop_io io={&hw,b,drain};hw.ctx=b;hw.qualified=qual;hw.read32=rd;hw.write32=wr;hw.delay_us=delay;return a04_thermal_stop(s,&io);}
int main(void){struct bus b={0};struct a04_thermal_stop_state s={0};unsigned i,ops;
 b.mask=0x80000000u;b.periodic=0xf;assert(!run(&b,&s));assert(s.irq_masked&&s.irq_drained&&s.ahb_idle&&s.periodic_off&&!s.quarantined);ops=b.ops;
 for(i=1;i<=ops;i++){memset(&b,0,sizeof(b));memset(&s,0,sizeof(s));b.fail=i;assert(run(&b,&s)&&s.touched&&s.quarantined);{unsigned before=b.ops;assert(run(&b,&s)&&b.ops==before);}}
 memset(&b,0,sizeof(b));memset(&s,0,sizeof(s));b.drain_fail=1;assert(run(&b,&s)&&s.irq_masked&&!s.irq_drained&&!s.periodic_off&&s.quarantined);
 memset(&b,0,sizeof(b));memset(&s,0,sizeof(s));b.busy=1;assert(run(&b,&s)&&!s.ahb_idle&&s.quarantined&&b.ops<110);
 memset(&b,0,sizeof(b));memset(&s,0,sizeof(s));b.lost=1;assert(run(&b,&s)&&!b.ops&&!s.touched);
 return 0;}
