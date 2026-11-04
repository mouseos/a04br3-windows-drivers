/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "mt8167_i2c.h"
struct fake { uint16_t regs[128]; unsigned delay, count; uint16_t data[4]; int mode; };
static uint16_t rd(void *p, unsigned r) { struct fake *f=p; if(r==0x24) return f->mode==4; if(r==0x0c) return f->mode==1?2:f->mode==2?0:1; if(r==0x30) return f->mode==3?0x20:0x10; if(r==0) return 0xa5; return f->regs[r/2]; }
static void wr(void *p,unsigned r,uint16_t v) { struct fake *f=p; f->regs[r/2]=v; if(r==0 && f->count<4) f->data[f->count++]=v; }
static void delay(void *p,unsigned us) { struct fake *f=p; assert(us==5); ++f->delay; }
static uint64_t now(void *p) { return (uint64_t)((struct fake *)p)->delay*5000; }
int main(void) {
 struct fake f; struct a04_i2c_io io={&f,rd,wr,delay}; struct a04_gauge_mt8167 a={&io,now,0}; uint8_t value=0; int mode;
 memset(&f,0,sizeof(f)); assert(!a04_bq_mt8167_read(&a,4,&value) && value==0xa5);
 assert(f.regs[2]==0xd6 && f.data[0]==4 && f.regs[0x6c/2]==1);
 memset(&f,0,sizeof(f)); assert(!a04_bq_mt8167_kick(&a,0x80)); assert(f.count==2 && f.data[0]==0 && f.data[1]==0x80);
 memset(&f,0,sizeof(f)); assert(a04_bq_mt8167_kick(&a,0x81) && f.count==0);
 for(mode=1;mode<=4;mode++){ memset(&f,0,sizeof(f)); f.mode=mode; a.faulted=0; value=0x55;
 assert(a04_bq_mt8167_read(&a,0,&value)); assert(a.faulted && value==0x55); f.mode=0;
 assert(a04_bq_mt8167_kick(&a,0x80)); }
 return 0;
}
