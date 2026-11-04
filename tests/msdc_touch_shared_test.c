/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "shared_board.h"
#include <assert.h>
#include <string.h>
struct fake { uint32_t regs[3][1024];unsigned locks,writes,i2cw;int locked,fail; };
static int lock(void *p){struct fake *f=p;assert(!f->locked);f->locked=1;f->locks++;return 0;}
static void unlock(void *p){struct fake *f=p;assert(f->locked);f->locked=0;}
static int rd(void *p,enum a04_shared_bank b,unsigned off,uint32_t *v){struct fake *f=p;assert(f->locked && off<4096 && b<=A04_SHARED_GPIO);if(f->fail)return -1;*v=f->regs[b][off/4];return 0;}
static int wr(void *p,enum a04_shared_bank b,unsigned off,uint32_t v){struct fake *f=p;assert(f->locked && off<4096 && b<=A04_SHARED_GPIO);f->writes++;if(f->fail)return -1;f->regs[b][off/4]=v;return 0;}
static int i2c(void *p,unsigned off,uint16_t v){struct fake *f=p;(void)off;(void)v;assert(!f->locked);f->i2cw++;return 0;}
int main(void)
{
 struct fake f={0};struct a04_register_owner owner={&f,lock,unlock,rd,wr,0,0};
 struct a04_touch_shared_board shared={&owner,&f,i2c};struct a04_touch_board_io io;int down=42;uint32_t old;
 assert(!a04_touch_shared_bind(&shared,&io));
 assert(a04_touch_board_init(&io)!=0 && !f.writes && !f.i2cw);
 assert(!a04_register_owner_start(&owner));f.regs[A04_SHARED_GPIO][0x3b0/4]=0xffff0000u;
 assert(!a04_touch_board_init(&io) && f.i2cw==7 && !f.locked);
 assert((f.regs[A04_SHARED_GPIO][0x3b0/4]&((7u<<9)|(7u<<12)))==((1u<<9)|(1u<<12)));
 assert((f.regs[A04_SHARED_GPIO][0x3b0/4]&0xffff0000u)==0xffff0000u);
 assert(!a04_touch_shared_pen(&shared,&down) && down==1);
 f.regs[A04_SHARED_GPIO][0x200/4]=1u<<8;assert(!a04_touch_shared_pen(&shared,&down)&&down==0);
 old=f.writes;assert(a04_register_touch_update(&owner,A04_SHARED_GPIO,0x460,7,1)!=0 && f.writes==old);
 assert(a04_register_touch_update(&owner,A04_SHARED_GPIO,0x3b0,7u<<9,2u<<9)!=0 && f.writes==old);
 assert(!a04_register_owner_stop(&owner));down=42;assert(a04_touch_shared_pen(&shared,&down)!=0 && down==42);
 assert(!a04_register_owner_start(&owner));f.fail=1;assert(a04_touch_shared_pen(&shared,&down)!=0 && owner.poisoned && down==42);
 f.fail=0;assert(a04_register_touch_input(&owner,&down)!=0 && down==42);
 assert(!a04_register_owner_stop(&owner));assert(a04_register_owner_start(&owner)!=0);
 return 0;
}
