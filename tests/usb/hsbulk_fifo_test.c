/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "pio_transfer.h"
struct mock {unsigned char reg[0x500],fifo[4096];unsigned out,in,writes,fail_offset;};
static unsigned char r8(void *p,unsigned a){struct mock*m=p;return a==0x24?m->fifo[m->in++]:m->reg[a];}
static unsigned short r16(void*p,unsigned a){struct mock*m=p;return m->reg[a]|((unsigned)m->reg[a+1]<<8);}
static void w8(void*p,unsigned a,unsigned char v){struct mock*m=p;m->writes++;if(a==m->fail_offset)return;if(a==0x24){assert(m->out<sizeof(m->fifo));m->fifo[m->out++]=v;}else m->reg[a]=v;}
static void w16(void*p,unsigned a,unsigned short v){struct mock*m=p;m->writes++;if(a==m->fail_offset)return;
 if((a==0x12&&(v&8))||(a==0x16&&(v&16)))v=0;
 m->reg[a]=(unsigned char)v;m->reg[a+1]=(unsigned char)(v>>8);}
int main(void){
 struct mock m={0};MT8167_USB20_IO io={&m,r8,r16,w8,w16};
 struct a04_pio_endpoint e={1,4,512,0x40,2,1,0,0};struct a04_pio_transfer t;
 unsigned char buf[1041],saved[1041];unsigned i,w;
 for(i=0;i<sizeof(buf);i++)buf[i]=(unsigned char)(i*7);memcpy(saved,buf,sizeof(buf));
 assert(!a04_pio_configure(&io,&t,&e,buf,1037));
 assert(m.out==512&&m.reg[0x62]==6&&m.reg[0x63]==6&&r16(&m,0x64)==8&&r16(&m,0x66)==72);
 assert(8*8==64&&72*8==64+512&&72*8+512==1088); /* EP0, TX, RX disjoint */
 w16(&m,0x12,0x2082);assert(!a04_pio_event(&io,&t)&&m.out==512&&!t.actual); /* NAK: no duplicate FIFO write */
 for(i=0;i<3;i++){w16(&m,0x12,(i&1)?0:0x100);assert(a04_pio_event(&io,&t)==(i==2?1:0));}
 assert(t.actual==1037&&m.out==1037&&!memcmp(m.fifo,saved,1037));assert(!a04_pio_abort(&io,&t));
 /* IN 512+7 ends short; canaries after actual stay unchanged. */
 e.direction=0;m.in=0;memcpy(m.fifo,saved,519);memset(buf,0xcc,sizeof(buf));
 assert(!a04_pio_configure(&io,&t,&e,buf,1037));w16(&m,0x16,1);w16(&m,0x18,512);
 assert(!a04_pio_event(&io,&t)&&t.actual==512);w16(&m,0x16,0x201);w16(&m,0x18,7);
 assert(a04_pio_event(&io,&t)==1&&t.actual==519&&!memcmp(buf,saved,519));
 for(i=519;i<sizeof(buf);i++)assert(buf[i]==0xcc);assert(!a04_pio_abort(&io,&t));
 /* Overflow packet is rejected before copying beyond caller shadow. */
 m.in=0;assert(!a04_pio_configure(&io,&t,&e,buf,511));w16(&m,0x16,1);w16(&m,0x18,512);
 assert(a04_pio_event(&io,&t)==-1&&!t.actual&&!m.in);assert(!a04_pio_abort(&io,&t));
 /* Halt and cancel leave the software transfer active until verified flush. */
 assert(!a04_pio_configure(&io,&t,&e,buf,512));w16(&m,0x16,0x40);
 assert(a04_pio_event(&io,&t)==A04_PIO_STALL&&t.active);assert(!a04_pio_abort(&io,&t)&&!t.active);
 /* OUT and IN zero length are actual USB ZLPs, not synthetic success. */
 e.direction=1;m.out=0;assert(!a04_pio_configure(&io,&t,&e,0,0)&&!m.out&&t.active);
 w16(&m,0x12,0);assert(a04_pio_event(&io,&t)==1&&!t.actual);assert(!a04_pio_abort(&io,&t));
 e.direction=0;assert(!a04_pio_configure(&io,&t,&e,0,0));w16(&m,0x16,1);w16(&m,0x18,0);
 assert(a04_pio_event(&io,&t)==1&&!t.actual);assert(!a04_pio_abort(&io,&t));
 /* Rejection has no writes for malformed packet/speed/type profiles. */
 w=m.writes;e.packet=64;assert(a04_pio_configure(&io,&t,&e,buf,512)<0&&m.writes==w);
 e.packet=512;e.speed=0x80;assert(a04_pio_configure(&io,&t,&e,buf,512)<0&&m.writes==w);
 e.speed=0x40;e.type=3;assert(a04_pio_configure(&io,&t,&e,buf,512)<0&&m.writes==w);
 /* Failed bank readback: no FIFO write or request command is issued. */
 for(i=0;i<4;i++){
  memset(&m,0,sizeof(m));e.type=2;m.fail_offset=i==0?0x62:i==1?0x63:i==2?0x64:0x66;
  memset(&t,0,sizeof(t));assert(a04_pio_configure(&io,&t,&e,buf,512)<0);
  assert(!m.out&&!m.reg[0x12]&&!m.reg[0x16]&&!t.active);
 }
 /* A prior request still active forbids bank rewrite; no assumed stop. */
 memset(&m,0,sizeof(m));m.reg[0x16]=0x20;assert(a04_pio_configure(&io,&t,&e,buf,512)<0&&m.writes==1);
 return 0;
}
