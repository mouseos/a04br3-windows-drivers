#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "pmic_read.h"
struct mock { uint16_t regs[256],resp; unsigned fsm,commands,writes,clr;int stuck,mismatch,stuck_after_write; };
static uint32_t rd(void *p,unsigned off){struct mock*m=p;if(off==0x9c)return 1;if(off==0xa4)return (m->stuck?3u:m->fsm)<<16 | m->resp;assert(0);return 0;}
static void wr(void *p,unsigned off,uint32_t v){struct mock*m=p;if(off==0xa8){assert(v==1);m->fsm=0;m->clr++;return;}assert(off==0xa0);m->commands++;unsigned a=(v>>16)&0x7fffu;assert(a<256);if(v>>31){m->writes++;if(m->stuck_after_write)m->stuck=1;m->regs[a]=v&0xffffu;if(m->mismatch)m->regs[a]^=0x0001;m->fsm=0;}else{m->resp=m->regs[a];m->fsm=6;}}
static void delay(void *p){(void)p;}
int main(void){struct mock m={0};struct a04_pmic_read b={&m,rd,wr,delay,2,0};uint16_t out=0;
 m.regs[0x64/2]=0xaabbu;assert(!a04_pmic_classd_update(&b,0x64,0x0f00,0) && m.regs[0x64/2]==0xa0bb && m.writes==1 && m.commands==3);
 assert(!a04_pmic_classd_read_gain(&b,0x64,&out) && out==0xa0bb);
 {unsigned count=m.commands;assert(a04_pmic_classd_update(&b,0x10c,0xe,0xe) && m.commands==count);assert(a04_pmic_classd_update(&b,0x64,0xffff,0) && m.commands==count);assert(a04_pmic_classd_read_gain(&b,0x52,&out) && m.commands==count);}
 {unsigned count=m.commands;assert(!a04_pmic_classd_update(&b,0x64,0x0f00,0) && m.commands==count+1 && m.writes==1);}
 m.mismatch=1;assert(a04_pmic_classd_update(&b,0x64,0xf0ff,0x2018) && b.poisoned);{unsigned count=m.commands;assert(a04_pmic_classd_update(&b,0x64,0x0f00,0) && m.commands==count);}
 memset(&m,0,sizeof(m));b.poisoned=0;m.stuck=1;assert(a04_pmic_classd_update(&b,0x64,0x0f00,0) && b.poisoned && !m.commands);
 memset(&m,0,sizeof(m));b.poisoned=0;m.regs[0x64/2]=0x0a00;m.stuck_after_write=1;
 assert(a04_pmic_classd_update(&b,0x64,0x0f00,0) && b.poisoned && m.writes==1 && m.commands==2);
 {unsigned count=m.commands;assert(a04_pmic_classd_update(&b,0x64,0x0f00,0) && m.commands==count);}
 return 0;
}
