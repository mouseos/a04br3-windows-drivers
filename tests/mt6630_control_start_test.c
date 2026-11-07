/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "control_start.h"
struct fake {uint8_t mem[512];uint32_t argument,status,response;unsigned polls,cmds,cleaned;};
static uint32_t readreg(void *context,unsigned off,unsigned width)
{struct fake *f=context;(void)width;if(off==0x0c)return f->status;if(off==0x40)return f->response;return 0;}
static void writereg(void *context,unsigned off,uint32_t v,unsigned width)
{
 struct fake *f=context;unsigned cmd,addr;(void)width;
 if(off==0x0c){f->status&=~v;return;}if(off==0x38){f->argument=v;return;}if(off!=0x34)return;
 cmd=v&63u;f->cmds++;f->status=0x100;
 switch(cmd){case 0:f->response=0;break;case 5:f->response=(2u<<28)|0x100000u;if(f->argument&&++f->polls>=2)f->response|=0x80000000u;break;
 case 3:f->response=0x12340000u;break;case 7:f->response=0;break;
 case 52:addr=(f->argument>>9)&0x1ffffu;assert(addr<sizeof(f->mem));f->response=f->mem[addr];break;default:assert(0);}
}
static void delay(void *p,unsigned us){(void)p;(void)us;}
static int cleanup(void *p){struct fake *f=p;f->cleaned++;return 0;}
int main(void)
{
 struct fake f={0};struct a04_wifi_owner o={0,1,1,1,1,1,0,0};struct a04_wifi_life life={0};struct a04_sdio_card_info card;
 struct a04_msdc2_pio p={&f,readreg,writereg,delay,&o,0,0};struct a04_wifi_life_ops ops={&f,cleanup,cleanup,cleanup,cleanup};
 uint8_t cis[]={0x20,4,0x7a,0x03,0x30,0x66,0xff},v=0;
 f.mem[10]=1;memcpy(f.mem+256,cis,sizeof(cis));assert(!a04_wifi_admit(&o));
 assert(!a04_wifi_control_start(&life,&p,0x100000,&ops,&card));
 assert(card.vendor==0x037a && card.device==0x6630 && card.cis_length==sizeof(cis));
 f.mem[0]=0x44;assert(!a04_wifi_life_cmd52(&life,0,0,0,0,&v) && v==0x44);
 assert(!a04_wifi_life_stop(&life) && f.cleaned==4);
 assert(a04_wifi_life_cmd52(&life,0,0,0,0,&v)<0);
 puts("MSDC2 control enumeration through teardown: OK");return 0;
}
