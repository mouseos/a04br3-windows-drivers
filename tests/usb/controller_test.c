#include <assert.h>
#include <string.h>
#include "controller.h"
typedef struct { unsigned char b[0x500]; unsigned index; unsigned char sz[2][2]; unsigned short addr[2][2]; int reject_enable; } Mock;
static unsigned char r8(void*p,unsigned a){return ((Mock*)p)->b[a];}
static unsigned short r16(void*p,unsigned a){Mock*m=p;return (unsigned short)(m->b[a]|m->b[a+1]<<8);}
static unsigned r32(void*p,unsigned a){Mock*m=p;return m->b[a]|m->b[a+1]<<8|m->b[a+2]<<16|((unsigned)m->b[a+3]<<24);}
static void w8(void*p,unsigned a,unsigned char x){Mock*m=p;if(a==0x0e)m->index=x;if(a==0x62||a==0x63)m->sz[m->index][a-0x62]=x;if(a==0x0a)m->b[a]&=(unsigned char)~x;else m->b[a]=x;}
static void w16(void*p,unsigned a,unsigned short x){Mock*m=p;if(m->reject_enable && a==6 && x)return;if(a==0x64||a==0x66)m->addr[m->index][(a-0x64)/2]=x;if(a==2||a==4){m->b[a]&=(unsigned char)~x;m->b[a+1]&=(unsigned char)~(x>>8);}else{m->b[a]=(unsigned char)x;m->b[a+1]=(unsigned char)(x>>8);}}
static void w32(void*p,unsigned a,unsigned x){Mock*m=p;unsigned i;for(i=0;i<4;i++)m->b[a+i]=(unsigned char)(x>>(8*i));}
int main(void){Mock m;MT8167_USB20_CONTROLLER c;MT8167_USB20_EVENTS e;memset(&m,0,sizeof(m));
 c.io.context=&m;c.io.read8=r8;c.io.read16=r16;c.io.write8=w8;c.io.write16=w16;c.read32=r32;c.write32=w32;
 assert(!Mt8167UsbPioReady(&c));m.b[0x274]=1;assert(Mt8167UsbPioReady(&c)<0);m.b[0x274]=0;
 assert(Mt8167UsbFifoInit(&c,127)==-1);assert(!Mt8167UsbFifoInit(&c,128));
 assert(m.sz[0][0]==3&&m.sz[0][1]==3&&m.addr[1][1]==8&&m.sz[1][1]==3);
 w32(&m,0xa4,0x207);m.b[2]=1;m.b[4]=2;m.b[0xa]=0x30;
 assert(!Mt8167UsbInterruptEnable(&c));assert(r32(&m,0xa4)==0x207 && !m.b[2] && !m.b[4] && !m.b[0xa]);
 m.b[0xa0]=7;m.b[2]=1;m.b[4]=2;m.b[0xa]=0x30;
 assert(Mt8167UsbInterruptCapture(&c,&e)==1&&e.usb==0x30&&e.tx==1&&e.rx==2);
 assert(!m.b[2]&&!m.b[4]&&!m.b[0xa]);assert(!Mt8167UsbInterruptCapture(&c,&e));
 Mt8167UsbInterruptDisable(&c);assert(r32(&m,0xa4)==0x200&&!m.b[6]&&!m.b[8]&&!m.b[0xb]);
 m.reject_enable=1;assert(Mt8167UsbInterruptEnable(&c)==-1);
 assert(r32(&m,0xa4)==0x200 && !m.b[6] && !m.b[8] && !m.b[0xb]);return 0;}
