#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "../drivers/bus/mt8167-shared/pwrap_owner.h"
struct mock { uint16_t pdn,con,bbpu,data;unsigned fsm,writes,triggers;int busy,locked; };
static uint32_t rd(void *ctx,unsigned off) {struct mock *m=ctx;assert(m->locked);if(off==0x9c)return 1;if(off==0xa4)return (m->fsm<<16)|m->data;return 0;}
static void wr(void *ctx,unsigned off,uint32_t x) {
 struct mock*m=ctx;unsigned reg;assert(m->locked);if(off==0xa8){m->fsm=0;return;}assert(off==0xa0);reg=((x>>16)&0x7fffu)*2u;
 if(x&0x80000000u){m->writes++;if(reg==0x802c)m->pdn=(uint16_t)x;else if(reg==0x803e)m->con=(uint16_t)x;else if(reg==0x803c){assert((uint16_t)x==1);m->triggers++;}else assert(0);m->fsm=0;}
 else {if(reg==0x802c)m->data=m->pdn;else if(reg==0x803e)m->data=m->con;else if(reg==0x8000)m->data=m->busy?0x40:m->bbpu;else assert(0);m->fsm=6;}
}
static void delay(void *ctx){(void)ctx;}
static int lock(void *ctx){struct mock *m=ctx;assert(!m->locked);m->locked=1;return 0;}
static void unlock(void *ctx){struct mock *m=ctx;assert(m->locked);m->locked=0;}
int main(void){struct mock m={0};struct a04_pwrap_owner o={.lock_context=&m,.lock=lock,.unlock=unlock,.bus={&m,rd,wr,delay,3,0},.online=1};struct a04_rtc32k_lease l={0};
 {struct a04_rtc32k_lease foreign={1,0};assert(a04_pwrap_owner_rtc32k_acquire(&o,&foreign)&&!o.rtc_lease&&!m.writes);}
 m.pdn=0xa200;m.con=0x1020;assert(!a04_pwrap_owner_rtc32k_acquire(&o,&l));assert(o.rtc_lease==&l && l.held&&!l.uncertain&&m.pdn==0xa300&&m.con==0x1000&&m.triggers==2);
 assert(a04_pwrap_owner_rtc32k_acquire(&o,&l));assert(a04_pwrap_owner_stop(&o)&&o.online);m.pdn|=0x400;assert(!a04_pwrap_owner_rtc32k_release(&o,&l));assert(!o.rtc_lease&&!l.held&&m.pdn==0xa600&&m.con==0x1000&&m.triggers==3);assert(!a04_pwrap_owner_stop(&o)&&!o.online);assert(!a04_pwrap_owner_start(&o));
 m.pdn|=0x100;{unsigned n=m.writes;assert(a04_pwrap_owner_rtc32k_acquire(&o,&l)&&m.writes==n);}
 memset(&m,0,sizeof(m));m.busy=1;assert(a04_pwrap_owner_rtc32k_acquire(&o,&l)&&o.bus.poisoned&&l.uncertain&&!l.held&&o.rtc_lease==&l);assert(a04_pwrap_owner_rtc32k_release(&o,&l));
 return 0;}
