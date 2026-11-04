/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "emmc_prepare.h"
#include <assert.h>
#include <string.h>
struct fake {int lease,pin_fail,clock_fail,host_fail,card_fail;unsigned fsm,addr,pinwrites,clockwrites,hostcalls,cardcalls,releases;uint16_t vemc,selector,vio;uint32_t mode[3],top1,top2,cfg,sdccfg,inten,dma,ints,cmd,arg,pos;uint8_t ext[512];uint64_t ticks;};
static int acquired(void *v,uint32_t g){struct fake *f=v;assert(g==7);if(f->lease)return 0;f->lease=1;return 1;}
static int owned(void *v,uint32_t g){struct fake *f=v;assert(g==7);return f->lease;}
static int quiet(void *v){return ((struct fake *)v)->lease;}
static int release(void *v,uint32_t g){struct fake *f=v;assert(g==7);f->lease=0;f->releases++;return 1;}
static int rail(void *v,struct a04_emmc_rails_raw *r){struct fake *f=v;assert(f->lease&&f->pinwrites==3);r->vemc_enable=f->vemc;r->vemc_selector=f->selector;r->vio18_enable=f->vio;return 0;}
static int pin(void *v,unsigned n,unsigned *mode){struct fake *f=v;assert(f->lease&&n>=110&&n<=120);*mode=1;return 0;}
static int clockrd(void *v,unsigned off,uint32_t *x){struct fake *f=v;assert(f->lease&&f->pinwrites==3);if(f->clock_fail)return -1;*x=off==0x24?f->top1:f->top2;return 0;}
static int clockwr(void *v,unsigned off,uint32_t x){struct fake *f=v;assert(f->lease);f->clockwrites++;if(off==0x84)f->top1&=~x;else {assert(off==0x6c);f->top2|=x;}return 0;}
static int held(void *v,uint32_t g){return owned(v,g);}
static int pread(void *v,unsigned off,uint32_t *out){struct fake *f=v;assert(f->lease);if(off==0x9c){*out=1;return 0;}assert(off==0xa4);*out=f->fsm<<16;if(f->fsm==6)*out|=f->addr==0x508?f->vemc:f->addr==0x52e?f->selector:f->vio;return 0;}
static int pwrite(void *v,unsigned off,uint32_t x){struct fake *f=v;assert(f->lease);if(off==0xa8){assert(f->fsm==6);f->fsm=0;return 0;}assert(off==0xa0&&!f->fsm);f->addr=((x>>16)&0x7fff)*2u;if(x>>31){assert(f->addr==0x508);f->vemc=(uint16_t)x;}else f->fsm=6;return 0;}
static int waitus(void *v,unsigned us){struct fake *f=v;assert(us==264);f->ticks+=264;f->vemc|=0x8000;return 0;}
static uint64_t now(void *v){return ((struct fake *)v)->ticks++;}
static int pinsafe(void *v,uint32_t g){struct fake *f=v;return owned(v,g)&&(f->vemc&0xc000)==0xc000;}
static int pinrd(void *v,unsigned off,uint32_t *out){struct fake *f=v;unsigned i=(off-0x460)/0x10;assert(i<3);*out=f->mode[i];return 0;}
static int pinupdate(void *v,unsigned off,uint32_t mask,uint32_t value){struct fake *f=v;unsigned i=(off-0x460)/0x10;assert(f->lease&&i<3);f->pinwrites++;if(f->pin_fail)return -1;f->mode[i]=(f->mode[i]&~mask)|value;return 0;}
static int hostprep(void *v,uint32_t g){struct fake *f=v;assert(owned(v,g)&&f->pinwrites==3&&f->clockwrites==2);f->hostcalls++;if(f->host_fail)return 0;f->cfg=0x89;f->ext[212]=42;return 1;}
static uint32_t hostrd(void *v,uint32_t off)
{
 struct fake *f=v;if(off==0)return f->cfg;if(off==0x30)return f->sdccfg;if(off==0x10)return f->inten;
 if(off==0x9c)return f->dma;
 if(off==0x3c)return 0;
 if(off==0xc)return f->ints | (f->cmd==8 && f->pos==512 ? 0x1000u:0);
 if(off==0x14)return f->cmd==8 && f->pos<512 ? 64:0;
 if(off==0x1c){uint32_t x=0;unsigned j;for(j=0;j<4;++j)x|=(uint32_t)f->ext[f->pos++]<<(8*j);return x;}
 if(off>=0x40 && off<=0x4c){if(f->cmd==1)return 0xc0ff8000u;if(f->cmd==2||f->cmd==10)return 0x10203040u+(off-0x40);return (4u<<9)|(1u<<8);}
 return 0;
}
static uint8_t hostrd8(void *v,uint32_t off){struct fake *f=v;assert(off==0x1c);return f->ext[f->pos++];}
static void hostwr(void *v,uint32_t off,uint32_t x)
{
 struct fake *f=v;assert(f->lease && f->hostcalls && (off==0xc||off==0x14||off==0x34||off==0x38||off==0x50));
 if(off==0xc)f->ints&=~x;
 if(off==0x38)f->arg=x;
 if(off==0x34){f->cmd=x&63u;if(!f->cmd)f->cardcalls++;f->ints|=(f->card_fail&&f->cmd==3)?0x400u:0x100u;}
}
static void delay(void *v,uint32_t us){struct fake *f=v;assert(us==1000||us==10000);f->ticks+=us;}
static void init(struct fake *f){memset(f,0,sizeof(*f));f->selector=0x80;f->vio=0xc000;f->top1=1u<<17;}
int main(void)
{
 struct fake f;union {struct a04_emmc_prepare_state state;uint8_t scratch[512];} overlap_test={0};struct a04_emmc_prepare_state state={0};struct a04_resume_observation out,old;uint8_t scratch[512];
 struct a04_board_ops b={&f,acquired,owned,rail,pin,clockrd,clockwr,quiet,release};
 struct a04_vemc_io v={&f,held,pread,pwrite,waitus,now,10000,264};
 struct a04_pinmux_ops p={&f,held,pinsafe,pinrd,pinupdate,0};
 struct a04_cold_authority a={&f,held,now,delay,100000,7,1};struct a04_msdc_io h={&f,hostrd,hostrd8,hostwr};
 struct a04_emmc_prepare_ops ops={&b,&v,&p,&a,&h,hostprep};
 init(&f);memset(&out,0xa5,sizeof(out));old=out;
 assert(a04_emmc_prepare(&state,&ops,1,1000,scratch,(struct a04_resume_observation *)(void *)scratch)==A04_PREPARE_INVALID && !f.lease);
 assert(a04_emmc_prepare(&overlap_test.state,&ops,1,1000,overlap_test.scratch,&out)==A04_PREPARE_INVALID && !f.lease);
 v.wait_us=0;assert(a04_emmc_prepare(&state,&ops,1,1000,scratch,&out)==A04_PREPARE_INVALID && !f.lease);v.wait_us=waitus;
 assert(a04_emmc_prepare(&state,&ops,1,1000,scratch,&out)==A04_PREPARE_OBSERVED && f.lease && !state.board.poisoned && f.cardcalls==1 && out.sectors==42 && !out.power_owned && !out.clock_stable && !f.releases);
 init(&f);memset(&state,0,sizeof(state));out=old;f.vio=0;assert(a04_emmc_prepare(&state,&ops,1,1000,scratch,&out)==A04_PREPARE_RAIL && state.board.poisoned && f.lease && !f.pinwrites && !memcmp(&out,&old,sizeof(out)));
 init(&f);memset(&state,0,sizeof(state));out=old;f.pin_fail=1;assert(a04_emmc_prepare(&state,&ops,1,1000,scratch,&out)==A04_PREPARE_PIN && state.board.poisoned && f.lease && !f.clockwrites && !memcmp(&out,&old,sizeof(out)));
 init(&f);memset(&state,0,sizeof(state));out=old;f.clock_fail=1;assert(a04_emmc_prepare(&state,&ops,1,1000,scratch,&out)==A04_PREPARE_CLOCK && state.board.poisoned && f.lease && !f.hostcalls && !memcmp(&out,&old,sizeof(out)));
 init(&f);memset(&state,0,sizeof(state));out=old;f.host_fail=1;assert(a04_emmc_prepare(&state,&ops,1,1000,scratch,&out)==A04_PREPARE_HOST && state.board.poisoned && f.lease && !f.cardcalls && !memcmp(&out,&old,sizeof(out)));
 init(&f);memset(&state,0,sizeof(state));out=old;f.card_fail=1;assert(a04_emmc_prepare(&state,&ops,1,1000,scratch,&out)==A04_PREPARE_CARD && state.board.poisoned && f.lease && !memcmp(&out,&old,sizeof(out)));
 assert(a04_board_release(&state.board,&b)==A04_BOARD_BUSY && !f.releases);
 return 0;
}
