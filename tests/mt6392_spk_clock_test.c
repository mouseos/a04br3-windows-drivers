#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "pmic_read.h"
struct mock { uint16_t base,resp;unsigned fsm,writes,commands;int ignore,fail_after_write; };
static uint32_t rd(void *p,unsigned off){struct mock*m=p;if(off==0x9c)return 1;if(off==0xa4)return (m->fail_after_write&&m->writes?3u:m->fsm)<<16 | m->resp;assert(0);return 0;}
static void wr(void *p,unsigned off,uint32_t v){struct mock*m=p;if(off==0xa8){assert(v==1);m->fsm=0;return;}assert(off==0xa0);m->commands++;unsigned address=((v>>16)&0x7fffu)*2;
 if(v>>31){m->writes++;assert(address==0x10a || address==0x10c);assert(!(v&0xfff1u));if(!m->ignore){if(address==0x10a)m->base|=v&0xe;else m->base&=(uint16_t)~(v&0xe);}m->fsm=0;}
 else {assert(address==0x108);m->resp=m->base;m->fsm=6;}}
static void delay(void*p){(void)p;}
int main(void){struct mock m={0};struct a04_pmic_read b={&m,rd,wr,delay,2,0};struct a04_pmic_spk_clock l={0};
 m.base=0x800a;assert(!a04_pmic_spk_clock_acquire(&b,&l));assert(l.held && l.original==0xa && m.base==0x8000 && m.writes==1);
 assert(a04_pmic_spk_clock_acquire(&b,&l));assert(!a04_pmic_spk_clock_release(&b,&l) && !l.held && m.base==0x800a && m.writes==2);
 memset(&m,0,sizeof(m));m.base=0x8000;assert(!a04_pmic_spk_clock_acquire(&b,&l) && l.held && !m.writes);assert(!a04_pmic_spk_clock_release(&b,&l) && !m.writes);
 memset(&m,0,sizeof(m));m.base=0xa;m.ignore=1;assert(a04_pmic_spk_clock_acquire(&b,&l) && b.poisoned && l.held && m.writes==1);
 {unsigned n=m.commands;assert(a04_pmic_spk_clock_release(&b,&l) && m.commands==n && l.held);}
 memset(&m,0,sizeof(m));memset(&l,0,sizeof(l));b.poisoned=0;m.base=0xa;m.fail_after_write=1;
 assert(a04_pmic_spk_clock_acquire(&b,&l) && b.poisoned && l.held && m.writes==1);
 memset(&m,0,sizeof(m));memset(&l,0,sizeof(l));b.poisoned=0;m.base=0xa;assert(!a04_pmic_spk_clock_acquire(&b,&l));m.ignore=1;
 assert(a04_pmic_spk_clock_release(&b,&l) && b.poisoned && l.held);
 memset(&m,0,sizeof(m));memset(&l,0,sizeof(l));b.poisoned=0;m.base=0xa;assert(!a04_pmic_spk_clock_acquire(&b,&l));m.base|=2;
 {unsigned n=m.writes;assert(a04_pmic_spk_clock_release(&b,&l) && b.poisoned && l.held && m.writes==n);}
 return 0;
}
