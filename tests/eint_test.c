/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "eint.h"
struct mock { uint32_t reg[512]; unsigned offsets[8]; uint32_t values[8]; unsigned writes; };
static uint32_t rd(void *p, unsigned offset) { struct mock *m=p; assert(offset/4<512); return m->reg[offset/4]; }
static void wr(void *p, unsigned offset, uint32_t value)
{
    struct mock *m=p; assert(m->writes<8); m->offsets[m->writes]=offset; m->values[m->writes++]=value;
}
int main(void)
{
    struct mock m = {0}; struct a04_eint_io io = {&m,rd,wr}; uint32_t pending=0;
    assert(a04_eint_configure(&io,8,A04_EDGE_FALLING)==0 && m.writes==5);
    assert(m.offsets[0]==0x0c0 && m.offsets[1]==0x380 && m.offsets[2]==0x1c0 &&
           m.offsets[3]==0x040 && m.offsets[4]==0x100 && m.values[0]==0x100);
    m=(struct mock){0}; m.reg[0x080/4]=1u<<28;
    assert(a04_eint_configure(&io,28,A04_LEVEL_LOW)==0 && m.writes==3);
    assert(m.offsets[0]==0x380 && m.offsets[1]==0x180 && m.offsets[2]==0x040);
    m=(struct mock){0};
    assert(a04_eint_mask(&io,168,1)==0 && m.offsets[0]==0x0d4 && m.values[0]==0x100);
    assert(a04_eint_ack(&io,169)==-1 && m.writes==1);
    m.reg[(5*4)/4]=0xffffffffu; m.reg[(0x080+5*4)/4]=0x10u;
    assert(a04_eint_pending(&io,5,0xffffffffu,&pending)==0 && pending==0x1efu);
    assert(a04_eint_pending(&io,6,0xffffffffu,&pending)==-1);
    puts("MT8167 EINT register tests passed");
    return 0;
}
