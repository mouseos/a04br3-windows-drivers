/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "keyboard_port.h"
typedef struct {
 unsigned char regs[0x500],fifo[64],out[1024];unsigned index,fi,fo,count,delays,reports,releases;
 unsigned short ep0,rx;int stuck;
} MOCK;
static unsigned char r8(void *p,unsigned a){MOCK*m=p;if(a==0x20||a==0x24){assert(m->fi<64);return m->fifo[m->fi++];}if(a==0x18)return (unsigned char)m->count;return m->regs[a];}
static unsigned short r16(void*p,unsigned a){MOCK*m=p;if(a==0x12)return m->ep0;if(a==0x16)return m->rx;if(a==0x18)return (unsigned short)m->count;return m->regs[a]|(unsigned short)(m->regs[a+1]<<8);}
static void w8(void*p,unsigned a,unsigned char v){MOCK*m=p;if(a==0xe)m->index=v;if(a==0x20){assert(m->fo<sizeof(m->out));m->out[m->fo++]=v;}else m->regs[a]=v;}
static void w16(void*p,unsigned a,unsigned short v){MOCK*m=p;if(a==0x12){if(!m->stuck)m->ep0=(v&0x100)?0:v;}else if(a==0x16)m->rx=(v&0x90)?0:(unsigned short)((m->rx&v&0x4d)|(v&~0x4du));else{m->regs[a]=(unsigned char)v;m->regs[a+1]=(unsigned char)(v>>8);}}
static void delay(void*p,unsigned ms){((MOCK*)p)->delays+=ms;}
static int report(void*p,const unsigned char*r){MOCK*m=p;assert(r[2]==4);++m->reports;return 0;}
static void release(void*p){++((MOCK*)p)->releases;}
static int event(KB_PORT*p,unsigned usb,unsigned tx,unsigned rx,unsigned now){MT8167_USB20_EVENTS e={usb,tx,rx};return KbPortEvents(p,e,now);}
static void init(KB_PORT*p,KB_SESSION*s,MOCK*m){MT8167_USB20_IO io={m,r8,r16,w8,w16};memset(m,0,sizeof(*m));assert(!KbPortInit(p,s,&io,m,delay,report,release));m->regs[0x60]=5;}
static unsigned connect(KB_PORT*p,MOCK*m,unsigned now)
{
 assert(!event(p,0x10,1,2,now) && p->stage==KB_PORT_DEBOUNCE && !m->fo);
 assert(!KbPortTick(p,now+99) && !(m->regs[1]&8));
 assert(!KbPortTick(p,now+100) && p->stage==KB_PORT_RESET && (m->regs[1]&8));
 assert(!KbPortTick(p,now+149) && (m->regs[1]&8));
 assert(!KbPortTick(p,now+150) && p->stage==KB_PORT_RECOVER && !(m->regs[1]&8));
 assert(!KbPortTick(p,now+159) && !m->fo);
 assert(!KbPortTick(p,now+160) && p->stage==KB_PORT_ENUM && m->fo==8);
 return now+160;
}
static int complete(KB_PORT*p,MOCK*m,const unsigned char*data,unsigned n,unsigned now)
{
 unsigned off=0;int result;m->ep0=0;assert(!event(p,0,1,0,now));
 while(off<n){unsigned len=n-off<8?n-off:8;memcpy(m->fifo,data+off,len);m->fi=0;m->count=len;m->ep0=1;assert(!event(p,0,1,0,now));off+=len;}
 m->ep0=0;result=event(p,0,1,0,now);return result;
}
static void enumerate(KB_PORT*p,MOCK*m,unsigned now)
{
 unsigned char d[18]={18,1,0,2,0,0,0,8,0x34,0x12,0x78,0x56,0,1,0,0,0,1};
 unsigned char c[]={9,2,25,0,1,1,0,0x80,50,9,4,0,0,1,3,1,1,0,7,5,0x81,3,8,0,10};
 assert(!complete(p,m,d,8,now));assert(!complete(p,m,0,0,now));assert(m->delays>=2);
 assert(!complete(p,m,d,18,now));assert(!complete(p,m,c,9,now));assert(!complete(p,m,c,25,now));
 assert(!complete(p,m,0,0,now));assert(complete(p,m,0,0,now)==1 && p->stage==KB_PORT_READY);
}
int main(void)
{
 MOCK m;KB_PORT p;KB_SESSION s;unsigned now;
 init(&p,&s,&m);now=connect(&p,&m,0xfffffff0u);enumerate(&p,&m,now);
 assert(!KbPortTick(&p,now) && p.rx_pending && (m.rx&0x20));
 m.rx=8;assert(!event(&p,0,0,2,now+1) && !p.rx_pending); /* NAK: retry, no false report */
 assert(!KbPortTick(&p,now+9) && !p.rx_pending);assert(!KbPortTick(&p,now+10) && p.rx_pending);
 m.rx=1;m.count=8;m.fi=0;memset(m.fifo,0,8);m.fifo[2]=4;
 assert(!event(&p,0,0,2,now+11) && m.reports==1 && !p.rx_pending);
 assert(!event(&p,0x20,1,2,now+12) && p.stage==KB_PORT_WAIT && !p.rx_pending && m.releases==2);
 /* Same powered host accepts a second plug, discarding stale endpoint causes. */
 m.fo=0;m.ep0=3;m.rx=0x21;now=connect(&p,&m,now+20);enumerate(&p,&m,now);
 assert(!KbPortTick(&p,now) && p.rx_pending);assert(KbPortTick(&p,now+2000)<0 && p.stage==KB_PORT_FAULT);
 init(&p,&s,&m);now=connect(&p,&m,0);m.ep0=0x80a;assert(!event(&p,0,1,0,now+1999)); /* stale IRQ cannot extend deadline */
 assert(KbPortTick(&p,now+2000)<0 && p.stage==KB_PORT_FAULT);
 init(&p,&s,&m);assert(!event(&p,0x10,0,0,0));m.ep0=3;m.stuck=1;
 assert(KbPortTick(&p,100)<0 && p.stage==KB_PORT_FAULT && m.delays==5);
 init(&p,&s,&m);assert(!event(&p,0x10,0,0,0));m.rx=0x2000;
 assert(KbPortTick(&p,100)<0); /* Cannot quiesce an unknown DMA owner. */
 init(&p,&s,&m);assert(event(&p,0x80,1,2,0)<0 && p.stage==KB_PORT_FAULT);
 init(&p,&s,&m);m.regs[0x60]=0x81;assert(!event(&p,0x10,0,0,0));assert(!KbPortTick(&p,100));assert(!KbPortTick(&p,150));assert(KbPortTick(&p,160)<0);
 return 0;
}
