/* SPDX-License-Identifier: GPL-2.0-only */
#include "oneshot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {uint32_t regs[0x500/4],raw[3];uint64_t now;int own,ready,busy,fail_write,report_fail,reports;};
static int qualified(void *p){return ((struct fake*)p)->own;}
static int ready(void *p){return ((struct fake*)p)->ready;}
static int clock_on(void *p){(void)p;return 0;}
static int delay(void *p,unsigned us){(void)p;(void)us;return 0;}
static int read32(void *p,uint32_t a,uint32_t *v){struct fake *f=p;unsigned off;
 if(a==0x10018604 || a==0x11003000){*v=0;return 0;}
 if(a<0x1100d000 || a>=0x1100d500)return -1;
 off=a-0x1100d000;
 if(off>=0x90 && off<=0x98){*v=f->regs[0x78/4]==0x1100d428?0xda1:f->raw[(off-0x90)/4];return 0;}
 if(off==0x418){*v=f->busy?0x10000:0;return 0;}*v=f->regs[off/4];return 0;}
static int write32(void *p,uint32_t a,uint32_t v){struct fake *f=p;if(f->fail_write)return -1;if(a>=0x1100d000 && a<0x1100d500)f->regs[(a-0x1100d000)/4]=v;return 0;}
static uint64_t now(void *p){return ((struct fake*)p)->now++;}
static int wait_ms(void *p,unsigned ms){((struct fake*)p)->now+=ms;return 0;}
static int set_ready(void *p,int value){((struct fake*)p)->ready=value;return 0;}
static int report(void *p,const struct a04_thermal_sample *s){struct fake *f=p;assert(f->regs[0]==0 && (f->regs[0x3c/4]&0x10e)==0x10e);assert(s->hottest_mc>=20000);f->reports++;return f->report_fail;}
int main(void){struct fake f={0};struct a04_thermal_cal c={10000,0,{8813,8813,8813},40,0};
 struct a04_thermal_hw h={&f,qualified,clock_on,ready,read32,write32,delay};struct a04_oneshot_io io={&h,&f,now,wait_ms,set_ready,report};
 f.own=f.ready=1;f.raw[0]=3610;f.raw[1]=3609;f.raw[2]=3608;
 assert(a04_thermal_oneshot(&io,&c)==A04_ONESHOT_OK && f.reports==1 && !f.ready);
 memset(&f,0,sizeof f);f.own=f.ready=1;f.raw[0]=3610;f.raw[1]=3609;f.raw[2]=3608;f.report_fail=1;
 assert(a04_thermal_oneshot(&io,&c)==A04_ONESHOT_REPORT_FAILED && f.reports==1 && f.regs[0]==0);
 memset(&f,0,sizeof f);f.own=f.ready=1;f.fail_write=1;
 assert(a04_thermal_oneshot(&io,&c)==A04_ONESHOT_SHUTDOWN_FAILED && !f.reports);
 memset(&f,0,sizeof f);f.own=f.ready=1;f.raw[0]=f.raw[1]=f.raw[2]=0xda1;
 assert(a04_thermal_oneshot(&io,&c)==A04_ONESHOT_TIMEOUT && !f.reports && f.regs[0]==0);
 memset(&f,0,sizeof f);f.own=f.ready=1;f.busy=1;
 assert(a04_thermal_oneshot(&io,&c)==A04_ONESHOT_SHUTDOWN_FAILED && !f.reports);
 memset(&f,0,sizeof f);assert(a04_thermal_oneshot(&io,&c)==A04_ONESHOT_INVALID && !f.reports);
 puts("one-shot: measurement, stale timeout, AHB stuck and ownership rejection OK");return 0;}
