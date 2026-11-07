/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "enumerate.h"
struct fake {uint8_t mem[512];unsigned calls,polls,delays;int fail;};
static int cmd(void *context,unsigned op,uint32_t arg,unsigned response,uint32_t *out)
{
 struct fake *f=context;unsigned addr;(void)response;f->calls++;
 if(f->fail && f->calls==(unsigned)f->fail)return -1;
 switch(op){case 0:*out=0;break;case 5:*out=(2u<<28)|0x00100000u;if(arg && ++f->polls>=2)*out|=0x80000000u;break;
 case 3:*out=0x12340000u;break;case 7:*out=0;break;
 case 52:addr=(arg>>9)&0x1ffffu;if(addr>=sizeof(f->mem))return -1;*out=f->mem[addr];break;default:return -1;}
 return 0;
}
static void delay(void *context,unsigned us){struct fake *f=context;assert(us==10000);f->delays++;}
int main(void)
{
 struct a04_wifi_owner owner={0,1,1,1,1,1,0,0};struct fake f={0};
 struct a04_sdio_enumeration_io io={&f,cmd,delay};struct a04_sdio_card_info out={0};
 uint8_t cis[]={0x20,4,0x7a,0x03,0x30,0x66,0xff};
 f.mem[9]=0;f.mem[10]=1;f.mem[11]=0;f.mem[0]=0x32;memcpy(f.mem+256,cis,sizeof(cis));
 assert(!a04_wifi_admit(&owner));assert(!a04_mt6630_enumerate(&owner,&io,0x00100000u,&out));
 assert(out.vendor==0x037a && out.device==0x6630 && out.functions==2 && out.rca==0x12340000u && out.revision==0x32 && f.delays==1);
 owner=(struct a04_wifi_owner){0,1,1,1,1,1,0,0};assert(!a04_wifi_admit(&owner));f.calls=0;f.polls=0;f.mem[260]^=1;out.vendor=0xabcd;
 assert(a04_mt6630_enumerate(&owner,&io,0x00100000u,&out)==-2 && owner.poisoned && out.vendor==0xabcd);
 owner=(struct a04_wifi_owner){0,1,1,1,1,1,0,0};assert(!a04_wifi_admit(&owner));f.calls=0;
 assert(a04_mt6630_enumerate(&owner,&io,0x01000000u,&out)==-1 && !f.calls && !owner.poisoned);
 puts("SDIO enumeration and board-qualified OCR: OK");return 0;
}
