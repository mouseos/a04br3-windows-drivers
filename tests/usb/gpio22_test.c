#include <assert.h>
#include "gpio22.h"
typedef struct { unsigned reg[256]; unsigned writes; } M;
static unsigned rd(void*p,unsigned o){return ((M*)p)->reg[o/4];}
static void wr(void*p,unsigned o,unsigned v){M*m=p; m->writes++; if(o==0x118)m->reg[0x110/4]&=~v;else if(o==0x114)m->reg[0x110/4]|=v;else if(o==0x14)m->reg[0x10/4]|=v;else assert(0);}
int main(void){M m={0};A04_GPIO_IO io={&m,rd,wr};m.reg[0x110/4]=0x81;assert(!A04Gpio22Prepare(&io));assert(m.reg[0x110/4]==0x81 && m.reg[0x10/4]==64);assert(!A04Gpio22Vbus(&io,1));assert(m.reg[0x110/4]==0xc1);assert(!A04Gpio22Vbus(&io,0));m.reg[0x340/4]=64;assert(A04Gpio22Vbus(&io,1)==-1);return 0;}
