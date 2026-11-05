/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "bringup.h"
#include "resource_lease.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {uint32_t reg[3][1024],arg,status,response;uint8_t mem[512];unsigned polls,cmds,cleaned,ops,same_calls,deny_same_on;int lease,lose_after_board;};
static int qual(void *p){return ((struct fake*)p)->lease;}
static int same(void *p,const struct a04_wifi_owner *o,unsigned g){struct fake *f=p;f->same_calls++;return f->lease&&f->same_calls!=f->deny_same_on&&o->active&&g==o->generation;}
static int rd(void *p,enum a04_wifi_board_bank b,unsigned off,uint32_t *v){struct fake *f=p;f->ops++;*v=f->reg[b][off/4];if(b==A04_WIFI_MSDC&&off==0){*v&=~4u;*v|=0x80;}return 0;}
static int wr(void *p,enum a04_wifi_board_bank b,unsigned off,uint32_t v){struct fake *f=p;f->ops++;f->reg[b][off/4]=v;return 0;}
static int wait(void *p,unsigned ms){struct fake *f=p;f->ops++;if(f->lose_after_board && ms==10 && f->ops>20)f->lease=0;return 0;}
static uint32_t pio_rd(void *p,unsigned off,unsigned width){struct fake *f=p;(void)width;if(off==0xc)return f->status;if(off==0x40)return f->response;return 0;}
static void pio_wr(void *p,unsigned off,uint32_t v,unsigned width){struct fake *f=p;unsigned cmd,addr;(void)width;
 if(off==0xc){f->status&=~v;return;}if(off==0x38){f->arg=v;return;}if(off!=0x34)return;
 cmd=v&63u;f->cmds++;f->status=0x100;switch(cmd){case 0:f->response=0;break;case 5:f->response=(2u<<28)|0x100000u;if(f->arg&&++f->polls>=2)f->response|=0x80000000u;break;case 3:f->response=0x12340000u;break;case 7:f->response=0;break;case 52:addr=(f->arg>>9)&0x1ffffu;assert(addr<sizeof f->mem);f->response=f->mem[addr];break;default:assert(0);}}
static void delay(void *p,unsigned us){(void)p;(void)us;}
static int lease_acquire(void *p,enum a04_lease_resource r){(void)p;(void)r;return 0;}
static int lease_release(void *p,enum a04_lease_resource r){(void)p;(void)r;return 0;}
static int lease_drain(void *p){(void)p;return 0;}
static int cleanup(void *p){((struct fake*)p)->cleaned++;return 0;}
int main(void){struct fake f={0};struct a04_wifi_owner owner={0,1,1,1,1,1,0,0};struct a04_wifi_life life={0};struct a04_wifi_board board={0};struct a04_sdio_card_info out;
 struct a04_wifi_board_io io={&f,qual,rd,wr,wait};struct a04_msdc2_pio pio={&f,pio_rd,pio_wr,delay,&owner,0,0};struct a04_wifi_life_ops ops={&f,cleanup,cleanup,cleanup,cleanup};
 const uint8_t cis[]={0x20,4,0x7a,0x03,0x30,0x66,0xff};f.lease=1;f.mem[10]=1;memcpy(f.mem+256,cis,sizeof cis);assert(!a04_wifi_admit(&owner));
 assert(a04_wifi_bringup(&board,&io,&life,&pio,0x80000000u,&ops,&out,same,&f)<0 && !f.ops && !f.cmds);
 assert(!a04_wifi_bringup(&board,&io,&life,&pio,0x100000,&ops,&out,same,&f));
 assert(board.ready&&out.vendor==0x037a&&out.device==0x6630&&f.cmds);
 {uint8_t value=0;f.mem[0]=0x44;assert(!a04_wifi_life_cmd52(&life,0,0,0,0,&value)&&value==0x44);}
 assert(!a04_wifi_life_stop(&life)&&f.cleaned==4);
 memset(&f,0,sizeof f);memset(&owner,0,sizeof owner);memset(&life,0,sizeof life);memset(&board,0,sizeof board);f.lease=1;owner.host=owner.gpio_clock=owner.combo=owner.firmware=owner.irq=1;assert(!a04_wifi_admit(&owner));
 memset(&out,0xa5,sizeof out);assert(a04_wifi_bringup(&board,&io,&life,&pio,0x100000,&ops,&out,same,&f)<0);assert(board.poisoned&&owner.poisoned&&out.vendor==0xa5a5&&!f.cleaned);
 memset(&f,0,sizeof f);memset(&owner,0,sizeof owner);memset(&life,0,sizeof life);memset(&board,0,sizeof board);f.lease=1;f.deny_same_on=3;f.mem[10]=1;memcpy(f.mem+256,cis,sizeof cis);owner.host=owner.gpio_clock=owner.combo=owner.firmware=owner.irq=1;assert(!a04_wifi_admit(&owner));
 memset(&out,0xa5,sizeof out);assert(a04_wifi_bringup(&board,&io,&life,&pio,0x100000,&ops,&out,same,&f)<0);assert(board.poisoned&&owner.poisoned&&life.state==A04_WIFI_LIFE_FAILED&&out.vendor==0xa5a5&&!f.cleaned);
 {struct a04_resource_lease lease={0};struct a04_lease_ops lops={&f,lease_acquire,lease_release,lease_drain,lease_drain};uint32_t generation;
  memset(&f,0,sizeof f);memset(&owner,0,sizeof owner);memset(&life,0,sizeof life);memset(&board,0,sizeof board);
  f.lease=1;f.mem[10]=1;memcpy(f.mem+256,cis,sizeof cis);
  assert(!a04_resource_lease_acquire(&lease,&lops,&generation));assert(!a04_resource_lease_bind_owner(&lease,&owner));pio.generation=generation;
  assert(!a04_wifi_bringup(&board,&io,&life,&pio,0x100000,&ops,&out,a04_resource_lease_same_owner,&lease));
  assert(out.vendor==0x037a && out.device==0x6630);
  assert(!a04_resource_lease_quiesce(&lease,generation));assert(!owner.active);
  assert(!a04_resource_lease_release(&lease,generation));
 }
 puts("board-to-SDIO bringup, teardown and failure isolation: OK");return 0;}
