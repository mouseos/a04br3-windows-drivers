/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "thermal.h"
#include "manager.h"
struct env { unsigned reads; uint32_t words[3],raw[3]; uint64_t timestamp; int locked, fail_read, fail_lock, fail_sample, fail_apply; enum a04_thermal_action action; };
static int rd(void *p,unsigned n,uint32_t *v) { struct env *e=p; assert(e->locked); e->reads++; if(e->fail_read || n<60 || n>62)return -1; *v=e->words[n-60]; return 0; }
static int acquire(void *p) { struct env *e=p; if(e->fail_lock)return -1; assert(!e->locked); e->locked=1;return 0; }
static void release(void *p) { struct env *e=p; assert(e->locked);e->locked=0; }
static int sample(void *p,uint32_t r[3],uint64_t *t) { struct env *e=p; assert(e->locked); if(e->fail_sample)return -1;memcpy(r,e->raw,sizeof(e->raw));*t=e->timestamp;return 0; }
static int apply(void *p,enum a04_thermal_action a) { struct env *e=p;assert(!e->locked);e->action=a;return e->fail_apply; }
int main(void) {
 struct env e={0}; struct a04_thermal_manager m={0}; struct a04_thermal_sample s, old; int32_t temp;
 struct a04_thermal_policy p={80000,70000,110000,500,0}; struct a04_thermal_ops o={&e,rd,acquire,release,sample,apply};
 e.words[0]=(260u<<17)|(260u<<8)|(40u<<1)|1; e.words[1]=(512u<<22)|(512u<<12)|4; e.words[2]=260u<<23;
 e.raw[0]=e.raw[1]=e.raw[2]=3610; e.timestamp=1000;
 assert(!a04_thermal_start(&m,&o,&p)); assert(e.reads==3); assert(!a04_thermal_convert(&m.cal,0,3610,&temp)); assert(temp==20000);
 { uint16_t threshold=0; assert(!a04_thermal_threshold(&m.cal,20000,&threshold)); assert(threshold>=3608 && threshold<=3612); assert(a04_thermal_threshold(&m.cal,160000,&threshold)); }
 assert(!a04_thermal_poll(&m,&o,1000,&s)); assert(e.action==A04_TEMP_NORMAL && s.hottest_mc==20000);
 old=s; e.timestamp=1000; assert(a04_thermal_poll(&m,&o,1501,&s)); assert(e.action==A04_TEMP_HALT && !m.ready && !memcmp(&s,&old,sizeof s));
 assert(!a04_thermal_start(&m,&o,&p)); e.fail_sample=1; assert(a04_thermal_poll(&m,&o,1000,&s)); assert(e.action==A04_TEMP_HALT); e.fail_sample=0;
 e.words[0]&=~1u; assert(a04_thermal_start(&m,&o,&p)); e.words[0]|=1;
 assert(!a04_thermal_start(&m,&o,&p)); e.fail_lock=1; assert(a04_thermal_poll(&m,&o,1000,&s)); assert(e.action==A04_TEMP_HALT); e.fail_lock=0;
 assert(!a04_thermal_start(&m,&o,&p)); e.fail_apply=1; assert(a04_thermal_poll(&m,&o,1000,&s)); assert(!m.ready); e.fail_apply=0;
 assert(!a04_thermal_start(&m,&o,&p)); e.raw[2]=0; assert(a04_thermal_poll(&m,&o,1000,&s)); assert(e.action==A04_TEMP_HALT);
 { struct a04_thermal_policy q=p; struct a04_thermal_sample z={{0},85000,10}; assert(a04_thermal_decide(&q,&z,10,1)==A04_TEMP_LIMIT); z.hottest_mc=75000; assert(a04_thermal_decide(&q,&z,10,1)==A04_TEMP_LIMIT); z.hottest_mc=70000; assert(a04_thermal_decide(&q,&z,10,1)==A04_TEMP_NORMAL); z.hottest_mc=110000; assert(a04_thermal_decide(&q,&z,10,1)==A04_TEMP_HALT); }
 puts("thermal calibration, conversion, freshness, hysteresis and failure paths: OK"); return 0;
}
