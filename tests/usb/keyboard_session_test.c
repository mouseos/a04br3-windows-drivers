#include <assert.h>
#include <string.h>
#include "keyboard_session.h"
typedef struct { unsigned char reg[0x500],fifo[512],out[512]; unsigned fi,fo,count,delay,reports; unsigned short csr; } Mock;
static unsigned char r8(void*p,unsigned a){Mock*m=p;if(a==0x20||a==0x24)return m->fifo[m->fi++];if(a==0x18)return (unsigned char)m->count;return m->reg[a];}
static unsigned short r16(void*p,unsigned a){Mock*m=p;if(a==0x12||a==0x16)return m->csr;if(a==0x18)return (unsigned short)m->count;return (unsigned short)(m->reg[a]|m->reg[a+1]<<8);}
static void w8(void*p,unsigned a,unsigned char v){Mock*m=p;if(a==0x20)m->out[m->fo++]=v;else m->reg[a]=v;}
static void w16(void*p,unsigned a,unsigned short v){Mock*m=p;if(a==0x12||a==0x16)m->csr=v;else {m->reg[a]=(unsigned char)v;m->reg[a+1]=(unsigned char)(v>>8);}}
static void delay(void*p,unsigned n){((Mock*)p)->delay+=n;}
static int report(void*p,const unsigned char *r){Mock*m=p;assert(r[2]==4);m->reports++;return 0;}
static int complete(KB_SESSION *s,Mock*m,const unsigned char *p,unsigned n)
{
 unsigned offset=0; int result; m->csr=0; result=KbSessionEp0Interrupt(s); assert(result==0);
 while(offset<n) { unsigned maxp=s->ep0.transfer.max_packet;unsigned packet=n-offset<maxp?n-offset:maxp; memcpy(m->fifo,p+offset,packet);m->fi=0;m->count=packet;m->csr=1;assert(KbSessionEp0Interrupt(s)==0);offset+=packet; }
 m->csr=0; return KbSessionEp0Interrupt(s);
}
int main(void)
{
 Mock m={0}; MT8167_USB20_IO io={&m,r8,r16,w8,w16}; KB_SESSION s;
 unsigned char d[18]={18,1,0,2,0,0,0,8,0x34,0x12,0x78,0x56,0,1,0,0,0,1};
 unsigned char c[]={9,2,25,0,1,1,0,0x80,50,9,4,0,0,1,3,1,1,0,7,5,0x81,3,8,0,10};
 assert(!KbSessionStart(&s,&io,0x80,&m,delay,report));
 assert(!complete(&s,&m,d,8)); assert(!complete(&s,&m,0,0)); assert(m.delay==2);
 assert(!complete(&s,&m,d,18)); assert(!complete(&s,&m,c,9)); assert(!complete(&s,&m,c,25));
 assert(!complete(&s,&m,0,0)); assert(complete(&s,&m,0,0)==1);
 m.csr=0; assert(!KbSessionRequestReport(&s)); m.csr=1;m.count=8;m.fi=0;memset(m.fifo,0,8);m.fifo[2]=4;
 assert(KbSessionRxInterrupt(&s)==1 && m.reports==1);
 /* Low/high-speed descriptors are checked before SET_ADDRESS is sent. */
 memset(&m,0,sizeof(m));d[7]=16;assert(!KbSessionStart(&s,&io,0xc0,&m,delay,report));
 assert(complete(&s,&m,d,8)<0 && s.enumeration.stage==KB_ERROR && m.fo==8);
 memset(&m,0,sizeof(m));d[7]=8;assert(!KbSessionStart(&s,&io,0x40,&m,delay,report));assert(s.ep0.transfer.max_packet==64);
 assert(complete(&s,&m,d,8)<0 && s.enumeration.stage==KB_ERROR && m.fo==8);
 memset(&m,0,sizeof(m));d[7]=64;assert(!KbSessionStart(&s,&io,0x40,&m,delay,report));
 assert(!complete(&s,&m,d,8));assert(!complete(&s,&m,0,0));assert(!complete(&s,&m,d,18));
 assert(!complete(&s,&m,c,9));assert(!complete(&s,&m,c,25));assert(!complete(&s,&m,0,0));assert(complete(&s,&m,0,0)==1);
 m.csr=0x40;assert(KbSessionRxInterrupt(&s)<0 && s.enumeration.stage==KB_ERROR);assert(KbSessionRequestReport(&s)<0);
 memset(&m,0,sizeof(m));assert(!KbSessionStart(&s,&io,0x80,&m,delay,report));m.csr=4;
 assert(KbSessionEp0Interrupt(&s)<0 && s.enumeration.stage==KB_ERROR);
 return 0;
}
