/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "pio_transfer.h"
struct mock {unsigned char reg[0x500],fifo[256];unsigned out,in,reads;};
static unsigned char r8(void *p,unsigned a){struct mock *m=p;m->reads++;return a==0x24?m->fifo[m->in++]:m->reg[a];}
static unsigned short r16(void *p,unsigned a){struct mock *m=p;return m->reg[a]|((unsigned)m->reg[a+1]<<8);}
static void w8(void *p,unsigned a,unsigned char v){struct mock *m=p;if(a==0x24)m->fifo[m->out++]=v;else m->reg[a]=v;}
static void w16(void *p,unsigned a,unsigned short v){struct mock*m=p;
 if((a==0x12&&(v&8))||(a==0x16&&(v&16)))v=0;
 m->reg[a]=(unsigned char)v;m->reg[a+1]=(unsigned char)(v>>8);}
int main(void)
{
 struct mock m={0};MT8167_USB20_IO io={&m,r8,r16,w8,w16};
 struct a04_pio_endpoint e={1,4,8,0x80,2,1,1,0};struct a04_pio_transfer t;
 unsigned char b[24];unsigned i;
 for(i=0;i<sizeof(b);i++)b[i]=(unsigned char)i;
 assert(!a04_pio_configure(&io,&t,&e,b,20));assert(m.out==8);
 w16(&m,0x12,0x100);assert(a04_pio_event(&io,&t)==0&&t.actual==8&&m.out==16);
 w16(&m,0x12,0);assert(a04_pio_event(&io,&t)==0&&t.actual==16&&m.out==20);
 w16(&m,0x12,0x100);assert(a04_pio_event(&io,&t)==1&&t.actual==20&&t.ep.toggle==1);
 assert(!memcmp(m.fifo,b,20));
 m.out=0;assert(!a04_pio_configure(&io,&t,&e,b,8));w16(&m,0x12,0x2082);
 assert(a04_pio_event(&io,&t)==0&&m.out==8&&t.actual==0); /* no duplicate FIFO bytes on NAK */
 w16(&m,0x12,0x20);assert(a04_pio_event(&io,&t)==-2);assert(!a04_pio_abort(&io,&t)&&!t.active);
 e.direction=0;e.type=3;m.in=0;assert(!a04_pio_configure(&io,&t,&e,b,20));
 memset(m.fifo,0x55,8);w16(&m,0x16,1);w16(&m,0x18,8);
 assert(a04_pio_event(&io,&t)==0&&t.actual==8);
 w16(&m,0x16,0x201);w16(&m,0x18,3);assert(a04_pio_event(&io,&t)==1&&t.actual==11);
 assert(!a04_pio_configure(&io,&t,&e,b,8));w16(&m,0x16,1);w16(&m,0x18,9);
 assert(a04_pio_event(&io,&t)==-1&&t.actual==0);assert(!a04_pio_abort(&io,&t));
 e.packet=512;assert(a04_pio_configure(&io,&t,&e,b,8)<0);e.packet=8;
 e.speed=0xc0;e.type=2;assert(a04_pio_configure(&io,&t,&e,b,8)<0);
 e.speed=0x80;e.direction=1;e.type=3;assert(!a04_pio_configure(&io,&t,&e,0,0));
 w16(&m,0x12,0);assert(a04_pio_event(&io,&t)==1&&t.actual==0);
 return 0;
}
