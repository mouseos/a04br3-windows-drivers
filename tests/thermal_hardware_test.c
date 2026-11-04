/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include "hardware.h"
struct fake { unsigned writes,reads,delays; int ownership, clocks, ready, stale, bad_bus, fail_after; };
static int q(void *p){ return ((struct fake *)p)->ownership; }
static int clocks(void *p){struct fake *f=p; f->clocks++; return 0;}
static int initialized(void *p){ return ((struct fake *)p)->ready; }
static int read32(void *p,uint32_t a,uint32_t *v){ struct fake *f=p;f->reads++; if(f->fail_after && f->reads>=(unsigned)f->fail_after)return -1; if(a>=0x1100d090 && a<=0x1100d098) *v=f->stale?0:0xda1; else if(a==0x1100d418) *v=f->bad_bus?0x10000:0; else *v=0; return 0; }
static int write32(void *p,uint32_t a,uint32_t v){ struct fake *f=p;if(a==0x1100d068 && v!=0x1100d420) assert(v==0x1100300c); if(a==0x1100d078 && v!=0x1100d428) assert(v==0x11003040);f->writes++;return 0; }
static int delay(void *p,unsigned t){struct fake *f=p;(void)t;f->delays++;return 0;}
int main(void){struct fake f={0};struct a04_thermal_hw h={&f,q,clocks,initialized,read32,write32,delay};
 assert(a04_thermal_hw_init(&h));assert(!f.clocks&&!f.writes&&!f.reads);
 f.ownership=1;assert(!a04_thermal_hw_init(&h));assert(f.writes>30&&f.clocks==1);
 { struct a04_thermal_cal c={10000,0,{8813,8813,8813},40,0}; unsigned before=f.writes; assert(a04_thermal_hw_protect(&h,&c,100000)); assert(before==f.writes); f.ready=1; assert(!a04_thermal_hw_protect(&h,&c,100000)); assert(f.writes==before+5); }
 f=(struct fake){0};f.ownership=1;f.stale=1;assert(a04_thermal_hw_init(&h));assert(f.delays>=100);
 f=(struct fake){0};f.ownership=1;f.bad_bus=1;assert(a04_thermal_hw_init(&h));assert(f.delays>=50);
 f=(struct fake){0};f.ownership=1;f.fail_after=2;assert(a04_thermal_hw_init(&h));
 puts("thermal initialization ownership, self-test, timeout and I/O failure: OK");return 0;}
