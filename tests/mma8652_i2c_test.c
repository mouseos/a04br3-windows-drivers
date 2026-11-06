/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "mt8167_i2c.h"
struct fake { uint16_t reg[128]; uint8_t fifo[7]; unsigned pos, delay, fault, busy, writes; };
static uint16_t rd(void *p,unsigned off) {
    struct fake *f=p;
    if (off==0x24) return (uint16_t)f->busy;
    if (off==0x0c) return f->fault ? 2 : 1;
    if (off==0x30) return 7u<<4;
    if (off==0) return f->fifo[f->pos++];
    return f->reg[off/2];
}
static void wr(void *p,unsigned off,uint16_t value) {
    struct fake *f=p; f->reg[off/2]=value; ++f->writes;
    if(off==0x38)f->pos=0;
}
static void delay(void *p,unsigned us) { ((struct fake *)p)->delay+=us; }
static uint64_t now(void *p) { struct fake *f=p; return (uint64_t)++f->delay*1000u; }
int main(void) {
    struct fake f={.fifo={1,2,3,4,5,6,7}};
    struct a04_i2c_io io={&f,rd,wr,delay}; struct a04_mma_i2c a={&io,now,0};
    uint8_t data[7]={0}; unsigned i;
    assert(!a04_mma_i2c_read(&a,0x1d,0,data,7));
    for (i=0;i<7;++i) assert(data[i]==i+1);
    assert(f.reg[0x04/2]==0x3a && f.reg[0x14/2]==1 && f.reg[0x6c/2]==7 && f.reg[0x18/2]==2);
    f.fault=1; data[0]=99; assert(a04_mma_i2c_read(&a,0x1d,0,data,7));
    assert(a.faulted && data[0]==99); f.fault=0;
    assert(a04_mma_i2c_read(&a,0x1d,0,data,7));
    f.busy=1; i=f.writes;
    assert(a04_mma_i2c_recover_stopped(&a) && a.faulted && f.writes==i);
    f.busy=0;
    assert(!a04_mma_i2c_recover_stopped(&a) && !a.faulted);
    assert(!a04_mma_i2c_read(&a,0x1d,0,data,7) && data[0]==1 && data[6]==7);
    puts("MMA8652 I2C transport tests passed"); return 0;
}
