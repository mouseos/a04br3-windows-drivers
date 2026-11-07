/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One portable owner/ordering regression, never electrical/SCO evidence. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../sideband.h"
struct owner {
 uint32_t regs[16],initial[16];unsigned leased,pins,dma,release,delay,updates;
 int busy,refusal_token,drain_fail,lost;
};
static int acquire(void *p,unsigned rate,unsigned channels,unsigned bits,unsigned *generation)
{
 struct owner *o=p;assert(rate==8000&&channels==1&&bits==16);
 if(o->busy){if(o->refusal_token){o->leased=1;*generation=7;}return -1;}
 o->leased=1;*generation=7;return 0;
}
static int live(void *p,unsigned g){struct owner *o=p;return o->leased&&g==7&&!o->lost;}
static int read_afe(void *p,unsigned g,unsigned off,uint32_t *v)
{struct owner *o=p;assert(live(p,g)&&off<64);*v=o->regs[off/4];return 0;}
static int update(void *p,unsigned g,unsigned off,uint32_t mask,uint32_t value)
{
 struct owner *o=p;assert(live(p,g));o->updates++;
 if(off==0x1c&&(mask&3)&&(value&3)){
  assert((o->regs[0x3c/4]&0x10001)==0x10001&&o->delay==1);
 }
 o->regs[off/4]=(o->regs[off/4]&~mask)|(value&mask);return 0;
}
static int pins(void *p,unsigned g,int enable)
{struct owner *o=p;assert(live(p,g));o->pins=(unsigned)enable;return 0;}
static int prepare(void *p,unsigned g,unsigned rate,unsigned channels,unsigned bits)
{assert(live(p,g)&&rate==8000&&channels==1&&bits==16);return 0;}
static int start(void *p,unsigned g)
{
 struct owner *o=p;assert(live(p,g)&&o->pins&&(o->regs[0x1c/4]&3)==3);
 assert((o->regs[0x24/4]&0x3060)==0x3020&&(o->regs[0x2c/4]&0x40));o->dma=1;return 0;
}
static int stop(void *p,unsigned g)
{struct owner *o=p;assert(live(p,g));if(o->drain_fail)return -1;o->dma=0;return 0;}
static int delay(void *p,unsigned g,unsigned us)
{struct owner *o=p;assert(live(p,g)&&us==100);o->delay++;return 0;}
static int release(void *p,unsigned g)
{
 struct owner *o=p;assert(live(p,g)&&!o->dma&&!o->pins);
 assert(!memcmp(o->regs,o->initial,sizeof(o->regs)));o->leased=0;o->release++;return 0;
}
static void reset(struct owner *o,struct a04_bt_sideband *s)
{
 memset(o,0,sizeof(*o));memset(s,0,sizeof(*s));
 /* Unrelated register bits must survive configuration and final restore. */
 for(unsigned i=0;i<16;i++)o->regs[i]=0x40000000u;
 memcpy(o->initial,o->regs,sizeof(o->regs));
}
int main(void)
{
 struct owner o;struct a04_bt_sideband s;
 struct a04_bt_sideband_ops ops={&o,acquire,live,read_afe,update,pins,prepare,start,stop,delay,release};
 reset(&o,&s);assert(!a04_bt_sideband_prepare(&s,&ops));assert(!a04_bt_sideband_start(&s));
 assert(!a04_bt_sideband_close(&s)&&s.state==A04_BT_SB_CLOSED&&o.release==1);
 reset(&o,&s);o.busy=1;assert(a04_bt_sideband_prepare(&s,&ops)==-1&&!o.updates&&!o.leased);
 o.busy=0;assert(!a04_bt_sideband_prepare(&s,&ops));assert(!a04_bt_sideband_close(&s)&&o.release==1);
 reset(&o,&s);o.busy=1;o.refusal_token=1;
 assert(a04_bt_sideband_prepare(&s,&ops)==-2&&s.state==A04_BT_SB_FAULT&&o.leased&&!o.release&&!o.updates);
 reset(&o,&s);assert(!a04_bt_sideband_prepare(&s,&ops));assert(!a04_bt_sideband_start(&s));
 o.drain_fail=1;unsigned updates=o.updates;
 assert(a04_bt_sideband_close(&s)==-2&&s.state==A04_BT_SB_FAULT&&o.leased&&o.dma&&!o.release&&updates==o.updates);
 reset(&o,&s);assert(!a04_bt_sideband_prepare(&s,&ops));o.lost=1;updates=o.updates;
 assert(a04_bt_sideband_start(&s)==-2&&o.leased&&!o.release&&updates==o.updates);
 puts("Synthetic sideband owner lifecycle, MRG order, field preservation and drain/liveness failure retention: OK");return 0;
}
