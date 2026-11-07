/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "keyboard_port.h"
#include "keyboard_rx.h"
#include "boot_hid.h"
typedef struct {
 unsigned char regs[0x500],fifo[64],tx[1024],last[9];
 unsigned fi,fo,count,reports,releases,delay;unsigned short ep0,rx;
} MOCK;
static unsigned char r8(void *p,unsigned a){MOCK*m=p;if(a==0x20||a==0x24){assert(m->fi<m->count);return m->fifo[m->fi++];}if(a==0x18)return (unsigned char)m->count;return m->regs[a];}
static unsigned short r16(void*p,unsigned a){MOCK*m=p;if(a==0x12)return m->ep0;if(a==0x16)return m->rx;if(a==0x18)return (unsigned short)m->count;return m->regs[a]|(unsigned short)(m->regs[a+1]<<8);}
static void w8(void*p,unsigned a,unsigned char v){MOCK*m=p;if(a==0x20){assert(m->fo<sizeof(m->tx));m->tx[m->fo++]=v;}else m->regs[a]=v;}
static void w16(void*p,unsigned a,unsigned short v){MOCK*m=p;if(a==0x12)m->ep0=(v&0x100)?0:v;else if(a==0x16)m->rx=(v&0x90)?0:(unsigned short)((m->rx&v&0x4d)|(v&~0x4du));else{m->regs[a]=(unsigned char)v;m->regs[a+1]=(unsigned char)(v>>8);}}
static void delay(void*p,unsigned ms){((MOCK*)p)->delay+=ms;}
static int report(void*p,const unsigned char*r){MOCK*m=p;assert(!A04BootHidEncode(A04_BOOT_MOUSE,r,m->last));++m->reports;return 0;}
static void release(void*p){MOCK*m=p;++m->releases;assert(!A04BootHidMouseSnapshot(0,m->last));}
static int event(KB_PORT*p,unsigned usb,unsigned tx,unsigned rx,unsigned now){MT8167_USB20_EVENTS e={usb,tx,rx};return KbPortEvents(p,e,now);}
static void init(KB_PORT*p,KB_SESSION*s,MOCK*m){MT8167_USB20_IO io={m,r8,r16,w8,w16};memset(m,0,sizeof(*m));assert(!KbPortInit(p,s,&io,m,delay,report,release));m->regs[0x60]=5;}
static unsigned connect(KB_PORT*p,MOCK*m,unsigned now,unsigned speed)
{
 m->regs[0x60]=(unsigned char)(5|(speed==0xc0?0x20:0));m->regs[1]=speed==0x40?0x10:0;
 assert(!event(p,0x10,1,2,now));assert(!KbPortTick(p,now+100));assert(!KbPortTick(p,now+150));assert(!KbPortTick(p,now+160));
 assert(p->stage==KB_PORT_ENUM);return now+160;
}
static int complete(KB_PORT*p,MOCK*m,const unsigned char*data,unsigned n,unsigned now)
{
 unsigned off=0;m->ep0=0;assert(!event(p,0,1,0,now));
 while(off<n){unsigned maxp=p->session->ep0.transfer.max_packet;unsigned len=n-off<maxp?n-off:maxp;memcpy(m->fifo,data+off,len);m->fi=0;m->count=len;m->ep0=1;assert(!event(p,0,1,0,now));off+=len;}
 m->ep0=0;return event(p,0,1,0,now);
}
static void enumerate(KB_PORT*p,MOCK*m,unsigned now,unsigned speed,unsigned maxp,unsigned interval)
{
 unsigned char d[18]={18,1,0,2,0,0,0,8,0x34,0x12,0x78,0x56,0,1,0,0,0,1};
 unsigned char c[]={9,2,34,0,1,3,0,0x80,50,9,4,7,0,1,3,1,2,0,9,0x21,0x11,1,0,1,0x22,50,0,7,5,0x83,3,3,0,10};
 if(speed==0x40)d[7]=64;
 c[31]=(unsigned char)maxp;c[33]=(unsigned char)interval;
 {int rc=complete(p,m,d,8,now);if(rc)fprintf(stderr,"first descriptor speed=%x actualspeed=%x stage=%u ep0stage=%u rc=%d\n",speed,p->session->speed,p->session->enumeration.stage,p->session->ep0.transfer.stage,rc);assert(!rc);}
 assert(!complete(p,m,0,0,now));assert(!complete(p,m,d,18,now));
 assert(!complete(p,m,c,9,now));assert(!complete(p,m,c,sizeof(c),now));
 assert(!memcmp(m->tx+m->fo-8,(unsigned char[]){0,9,3,0,0,0,0,0},8)); /* real configuration */
 assert(!complete(p,m,0,0,now));
 assert(!memcmp(m->tx+m->fo-8,(unsigned char[]){0x21,11,0,0,7,0,0,0},8)); /* boot protocol, actual interface */
 assert(complete(p,m,0,0,now)==1 && p->stage==KB_PORT_READY);
 assert(p->session->enumeration.protocol==A04_BOOT_MOUSE && p->session->enumeration.endpoint==3);
 assert(m->regs[0x1c]==(speed|0x33u) && m->regs[0x1d]==interval && m->regs[0x14]==maxp);
}
static void parser(void)
{
 unsigned char d[18]={18,1,0,2,0,0,0,8,0,0,0,0,0,0,0,0,0,1};
 unsigned char good[]={9,2,25,0,1,1,0,0x80,50,9,4,5,0,1,3,1,2,0,7,5,0x82,3,3,0,10};
 unsigned edits[][2]={{12,1},{14,0xff},{15,0},{16,3},{20,2},{20,0x90},{21,1},{22,2},{22,65},{23,8},{24,0}};
 unsigned i;KB_ENUM k;
 for(i=0;i<=sizeof(edits)/sizeof(edits[0]);++i){unsigned char c[sizeof(good)];memcpy(c,good,sizeof(c));
  KbEnumStart(&k);assert(!KbControlComplete(&k,d,8));assert(!KbControlComplete(&k,0,0));assert(!KbControlComplete(&k,d,18));assert(!KbControlComplete(&k,c,9));
  if(i<sizeof(edits)/sizeof(edits[0])){c[edits[i][0]]=(unsigned char)edits[i][1];assert(KbControlComplete(&k,c,sizeof(c))<0 && k.stage==KB_ERROR);}
  else assert(!KbControlComplete(&k,c,sizeof(c)) && k.protocol==A04_BOOT_MOUSE && k.interface_number==5);
 }
 /* Existing keyboard wins when a composite advertises both interfaces. */
 {unsigned char c[41];memcpy(c,good,25);memcpy(c+25,good+9,16);c[2]=41;c[4]=2;c[27]=8;c[32]=1;c[38]=8;
  KbEnumStart(&k);assert(!KbControlComplete(&k,d,8));assert(!KbControlComplete(&k,0,0));assert(!KbControlComplete(&k,d,18));assert(!KbControlComplete(&k,c,9));assert(!KbControlComplete(&k,c,sizeof(c)));assert(k.protocol==A04_BOOT_KEYBOARD&&k.interface_number==8);
 }
}
int main(void)
{
 MOCK m;KB_PORT p;KB_SESSION s;unsigned speed,n,now;
 parser();
 for(speed=0;speed<3;++speed){unsigned bits=(unsigned[]){0xc0,0x80,0x40}[speed];
  init(&p,&s,&m);now=connect(&p,&m,0xfffffff0u,bits);enumerate(&p,&m,now,bits,8,10);
  for(n=3;n<=8;++n){assert(!KbPortTick(&p,now) && p.rx_pending);memset(m.fifo,0xee,n);m.fifo[0]=0xff;m.fifo[1]=0x80;m.fifo[2]=0x7f;m.fi=0;m.count=n;m.rx=1;
   assert(!event(&p,0,0,2,now) && !p.rx_pending && m.fi==n);
   assert(!memcmp(m.last,(unsigned char[]){2,7,0x80,0x7f,0,0,0,0,0},9));now+=100;
  }
  assert(!KbPortTick(&p,now) && p.rx_pending);assert(!KbPortTick(&p,now+300000));m.rx=8;assert(!event(&p,0,0,2,now+300001) && !p.rx_pending);
  assert(!event(&p,0x20,1,2,now+300002) && p.stage==KB_PORT_WAIT && !p.rx_pending);
  assert(!memcmp(m.last,(unsigned char[]){2,0,0,0,0,0,0,0,0},9));
  m.fo=0;now=connect(&p,&m,now+300100,bits);enumerate(&p,&m,now,bits,3,10);
  assert(!KbPortTick(&p,now));m.rx=1;m.fi=0;m.count=4;assert(event(&p,0,0,2,now)<0 && !m.fi); /* RXCOUNT exceeds actual endpoint */
 }
 for(n=0;n<=9;++n){MT8167_USB20_IO io;unsigned char boot[8];init(&p,&s,&m);io=(MT8167_USB20_IO){&m,r8,r16,w8,w16};m.rx=1;m.count=n;memset(boot,0x5a,8);
  if(n<3||n>8){assert(KbRxCompleteBoot(&io,boot,A04_BOOT_MOUSE,8)<0 && !m.fi && boot[0]==0x5a);}
 }
 init(&p,&s,&m);now=connect(&p,&m,0,0x40);enumerate(&p,&m,now,0x40,8,16);assert(!KbPortTick(&p,now));assert(!KbPortTick(&p,now+4096));m.rx=0x40;assert(event(&p,0,0,2,now+4097)<0);
 return 0;
}
