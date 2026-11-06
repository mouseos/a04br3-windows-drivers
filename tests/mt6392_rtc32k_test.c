#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../drivers/input/a04br3-buttons/pmic_read.h"
struct mock { uint16_t pdn,con,bbpu,data;unsigned fsm,writes,triggers;int busy; };
static uint32_t rd(void *ctx,unsigned off) {struct mock *m=ctx;if(off==0x9c)return 1;if(off==0xa4)return (m->fsm<<16)|m->data;return 0;}
static void wr(void *ctx,unsigned off,uint32_t x) {
 struct mock*m=ctx;unsigned reg;if(off==0xa8){m->fsm=0;return;}assert(off==0xa0);reg=((x>>16)&0x7fffu)*2u;
 if(x&0x80000000u){m->writes++;if(reg==0x802c)m->pdn=(uint16_t)x;else if(reg==0x803e)m->con=(uint16_t)x;else if(reg==0x803c){assert((uint16_t)x==1);m->triggers++;}else assert(0);m->fsm=0;}
 else {if(reg==0x802c)m->data=m->pdn;else if(reg==0x803e)m->data=m->con;else if(reg==0x8000)m->data=m->busy?0x40:m->bbpu;else assert(0);m->fsm=6;}
}
static void delay(void *ctx){(void)ctx;}
int main(void){struct mock m={0};struct a04_pmic_read b={&m,rd,wr,delay,3,0};struct a04_rtc32k_lease l={0};
 m.pdn=0xa200;m.con=0x1020;assert(!a04_pmic_rtc32k_acquire(&b,&l));assert(l.held&&!l.uncertain&&m.pdn==0xa300&&m.con==0x1000&&m.triggers==2);
 assert(a04_pmic_rtc32k_acquire(&b,&l));m.pdn|=0x400;assert(!a04_pmic_rtc32k_release(&b,&l));assert(!l.held&&m.pdn==0xa600&&m.con==0x1000&&m.triggers==3);
 m.pdn|=0x100;{unsigned n=m.writes;assert(a04_pmic_rtc32k_acquire(&b,&l)&&m.writes==n);}
 memset(&m,0,sizeof(m));m.busy=1;assert(a04_pmic_rtc32k_acquire(&b,&l)&&b.poisoned&&l.uncertain&&!l.held);assert(a04_pmic_rtc32k_release(&b,&l));
 return 0;}
