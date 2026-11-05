/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "mt8167_i2c.h"
struct fake { uint16_t regs[128]; unsigned delay, count, accesses, clocks; uint16_t data[4]; int mode; };
static uint16_t rd(void *p, unsigned r) { struct fake *f=p; ++f->accesses; if(r==0x24) return f->mode==4; if(r==0x0c) return f->mode==1?2:f->mode==2?0:1; if(r==0x30) return f->mode==3?0x20:f->mode==5?0x30:0x10; if(r==0) return 0xa5; return f->regs[r/2]; }
static void wr(void *p,unsigned r,uint16_t v) { struct fake *f=p; ++f->accesses; f->regs[r/2]=v; if(r==0 && f->count<4) f->data[f->count++]=v; }
static void delay(void *p,unsigned us) { struct fake *f=p; assert(us==5); ++f->delay; }
static uint64_t now(void *p) { struct fake *f=p; ++f->clocks; if(f->mode==6) return f->clocks==1?100:f->clocks==2?120:110; return (uint64_t)f->delay*5000; }
int main(void) {
 struct fake f; struct a04_i2c_io io={&f,rd,wr,delay}; struct a04_gauge_mt8167 a={.io=&io,.monotonic_ns=now}; uint8_t value=0; int mode;
 memset(&f,0,sizeof(f)); assert(!a04_bq_mt8167_read(&a,4,&value) && value==0xa5);
 assert(f.regs[2]==0xd6 && f.data[0]==4 && f.regs[0x6c/2]==1);
 memset(&f,0,sizeof(f)); assert(!a04_bq_mt8167_kick(&a,0x80)); assert(f.count==2 && f.data[0]==0 && f.data[1]==0x80);
 memset(&f,0,sizeof(f)); assert(a04_bq_mt8167_kick(&a,0x81) && f.count==0);
 for(mode=1;mode<=4;mode++){ memset(&f,0,sizeof(f)); f.mode=mode; a.faulted=0; value=0x55;
 assert(a04_bq_mt8167_read(&a,0,&value)); assert(a.faulted && value==0x55);
 assert(a.fault_reason==(unsigned)(mode==4?1:mode==1?2:mode==2?4:3)); f.mode=0;
 assert(a04_bq_mt8167_kick(&a,0x80)); }
 memset(&f,0,sizeof(f));a.faulted=0;f.mode=6;value=0x55;
 assert(a04_bq_mt8167_read(&a,0,&value));assert(a.faulted&&f.clocks==3&&value==0x55);
 /* A single poisoned adapter blocks both device addresses, without further
  * MMIO or an attempted controller reset, in either failure direction. */
 { unsigned accesses; uint8_t gauge[3]={1,2,3};
   memset(&f,0,sizeof(f));a.faulted=0;f.mode=1;
   assert(a04_bq_mt8167_read(&a,2,&value));accesses=f.accesses;f.mode=5;
   assert(a04_gauge_mt8167_read(&a,0x0b,9,gauge));assert(f.accesses==accesses&&gauge[0]==1);
   memset(&f,0,sizeof(f));a.faulted=0;f.mode=1;
   assert(a04_gauge_mt8167_read(&a,0x0b,9,gauge));accesses=f.accesses;f.mode=0;
   assert(a04_bq_mt8167_kick(&a,0x80));assert(f.accesses==accesses);
 }
 return 0;
}
