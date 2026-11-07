/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "msdc2_pio.h"
#include "transport.h"
#include "direct.h"
#include "lifetime.h"
struct fake {unsigned commands,reads,writes,delays; uint32_t status,r5,arg,command; int busy;};
static uint32_t rd(void *p,unsigned off,unsigned width)
{struct fake *f=p;(void)width; if(off==0x3c)return f->busy?3:0;if(off==0x0c)return f->status;if(off==0x14)return f->commands?4:0;if(off==0x40)return f->r5;if(off==0x1c){f->reads++;return 0x44332211;}return 0;}
static void wr(void *p,unsigned off,uint32_t v,unsigned width)
{struct fake *f=p;(void)width;f->writes++;if(off==0x38)f->arg=v;if(off==0x34){f->command=v;f->commands++;}if(off==0x0c)f->status&=~v;}
static void delay(void *p,unsigned us){struct fake *f=p;(void)us;f->delays++;if(f->commands&&!f->status)f->status=0x1100;}
static int done(void *p){(void)p;return 0;}
int main(void)
{
 struct a04_wifi_owner owner={0,1,1,1,1,1,0,0};struct fake f={0};
 struct a04_msdc2_pio p={&f,rd,wr,delay,&owner,0,0};
 struct a04_sdio_transport t={{&p,a04_msdc2_cmd53},1,0};uint8_t data[4]={0};
 assert(!a04_wifi_admit(&owner));
 assert(!a04_sdio_transfer(&t,0,1,1,0x10,data,4));
 assert(f.commands==1 && (f.command&63)==53 && f.reads==1 && data[0]==0x11 && data[3]==0x44);
 f.r5=1u<<9;f.status=0;assert(a04_sdio_transfer(&t,0,1,1,0x10,data,4)==-2);
 assert(t.poisoned && p.poisoned && owner.poisoned);
 assert(a04_sdio_transfer(&t,0,1,1,0x10,data,4)==-1 && f.commands==2);
 owner=(struct a04_wifi_owner){0,1,1,1,1,1,0,0};f=(struct fake){0};p.poisoned=0;p.generation=0;
 assert(!a04_wifi_admit(&owner));f.busy=1;
 assert(a04_msdc2_cmd53(&p,4,0,data,4)==-2 && owner.poisoned && f.commands==0 && f.delays==10000);
 owner=(struct a04_wifi_owner){0,1,1,1,1,1,0,0};f=(struct fake){0};p.poisoned=0;p.generation=0;
 assert(!a04_wifi_admit(&owner));
 {struct a04_sdio_direct d={&owner,&p,a04_msdc2_cmd52,0,0}; uint8_t value=0;
  f.r5=0x5a;assert(!a04_sdio_direct52(&d,0,1,0,0,&value));assert(value==0x5a && (f.command&63)==52);
  f.r5=1u<<9;assert(a04_sdio_direct52(&d,0,1,0,0,&value)==-2 && owner.poisoned);
 }
 owner=(struct a04_wifi_owner){0,1,1,1,1,1,0,0};f=(struct fake){0};p.poisoned=0;p.generation=0;
 assert(!a04_wifi_admit(&owner));
 {uint8_t cis[]={0x20,4,0x7a,0x03,0x30,0x66,0xff},v=0;
  struct a04_sdio_direct d={&owner,&p,a04_msdc2_cmd52,0,0};struct a04_sdio_bus b={&p,a04_msdc2_cmd53};
  struct a04_wifi_life life={0};struct a04_wifi_life_ops ops={&f,done,done,done,done};
  assert(!a04_wifi_life_begin(&life,&owner,cis,sizeof(cis),&d,&b,&ops));
  f.r5=0x31;assert(!a04_wifi_life_cmd52(&life,0,1,0,0,&v) && v==0x31);
  f.r5=0;assert(!a04_wifi_life_cmd53(&life,0,1,1,0,data,4));
  assert(!a04_wifi_life_stop(&life));assert(a04_wifi_life_cmd52(&life,0,1,0,0,&v)<0 && f.commands==2);
 }
 puts("MSDC2 CMD52/CMD53 mock transfer, lifetime and poison: OK");return 0;
}
