#include <assert.h>
#include "gpio22.h"
#include "board_pins.h"
typedef struct { unsigned reg[1024]; unsigned writes; } M;
static unsigned rd(void*p,unsigned o){return ((M*)p)->reg[o/4];}
static void wr(void*p,unsigned o,unsigned v){M*m=p; m->writes++; if(o==0x118)m->reg[0x110/4]&=~v;else if(o==0x114)m->reg[0x110/4]|=v;else if(o==0x14)m->reg[0x10/4]|=v;else if(o==0x28)m->reg[0x20/4]&=~v;else if(o==0x904)m->reg[0x900/4]|=v;else if(o==0x340||o==0x360||o==0x380)m->reg[o/4]=v;else assert(0);}
int main(void){M m={0};A04_GPIO_IO io={&m,rd,wr};m.reg[0x110/4]=0x81;assert(!A04Gpio22Prepare(&io));assert(m.reg[0x110/4]==0x81 && m.reg[0x10/4]==64);assert(!A04Gpio22Vbus(&io,1));assert(m.reg[0x110/4]==0xc1);assert(!A04Gpio22Vbus(&io,0));m.reg[0x340/4]=64;assert(A04Gpio22Vbus(&io,1)==-1);m.reg[0x340/4]=~0u;m.reg[0x360/4]=~0u;m.reg[0x380/4]=~0u;m.reg[0x20/4]=~0u;
 assert(!A04UsbPinsPrepare(&io));assert(m.reg[0x340/4]==(~0u&~(7u<<6)));assert(m.reg[0x360/4]==(~0u&~(7u<<12)));
 assert((m.reg[0x380/4]&(7u<<3))==(2u<<3));assert(!(m.reg[0x20/4]&((1u<<2)|(1u<<9))));assert(!(m.reg[0x110/4]&(1u<<6)));
 return 0;}
