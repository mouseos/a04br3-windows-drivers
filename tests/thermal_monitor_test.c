/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "monitor.h"
struct fake { uint32_t regs[0x500/4],raw[3];int own,ready,fail_apply,fail_halt,halts;enum a04_thermal_action last; };
static int q(void *p){return ((struct fake *)p)->own;}static int ready(void *p){return ((struct fake *)p)->ready;}
static int clk(void *p){(void)p;return 0;}static int delay(void *p,unsigned us){(void)p;(void)us;return 0;}
static int rd(void *p,uint32_t a,uint32_t *v){struct fake *f=p;unsigned off;
 if(a==0x10018604 || a==0x11003000){*v=0;return 0;}if(a<0x1100d000 || a>=0x1100d500)return -1;off=a-0x1100d000;
 if(off>=0x90 && off<=0x98){*v=f->regs[0x78/4]==0x1100d428?0xda1:f->raw[(off-0x90)/4];return 0;}
 if(off==0x418){*v=0;return 0;}*v=f->regs[off/4];return 0;}
static int wr(void *p,uint32_t a,uint32_t v){struct fake *f=p;if(a>=0x1100d000 && a<0x1100d500)f->regs[(a-0x1100d000)/4]=v;return 0;}
static int apply(void *p,enum a04_thermal_action a){struct fake *f=p;f->last=a;if(a==A04_TEMP_HALT)f->halts++;return (f->fail_apply && a!=A04_TEMP_HALT) || (f->fail_halt && a==A04_TEMP_HALT);}
int main(void){struct fake f={0};struct a04_monitor m={0};struct a04_thermal_sample out={{0},0,0};
 struct a04_thermal_cal c={10000,0,{8813,8813,8813},40,0};struct a04_thermal_policy p={80000,70000,110000,50,0};
 struct a04_thermal_hw h={&f,q,clk,ready,rd,wr,delay};
 f.own=f.ready=1;f.raw[0]=3610;f.raw[1]=3609;f.raw[2]=3608;
 assert(!a04_monitor_start(&m,&h,&c,&p,&f,apply,100,100));assert(a04_monitor_poll(&m,101,&out)==1);assert(out.hottest_mc>=20000);
 f.own=0;assert(a04_monitor_poll(&m,102,&out)==-1);assert(f.halts==1 && !m.active);
 memset(&f,0,sizeof(f));f.own=f.ready=1;f.raw[0]=f.raw[1]=f.raw[2]=3610;f.fail_apply=1;
 assert(!a04_monitor_start(&m,&h,&c,&p,&f,apply,200,100));assert(a04_monitor_poll(&m,201,&out)==-1);assert(f.halts==1);
 memset(&f,0,sizeof(f));f.own=0;assert(a04_monitor_start(&m,&h,&c,&p,&f,apply,0,100)==-1);assert(f.halts==1);
 memset(&f,0,sizeof(f));f.fail_halt=1;assert(a04_monitor_start(&m,&h,&c,&p,&f,apply,0,100)==-2);
 puts("thermal monitor ownership, valid readings and fail-closed stop: OK");return 0;}
