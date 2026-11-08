/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "keyboard_port.h"
#define INDEX 0x0eu
#define POWER 1u
#define DEVCTL 0x60u
#define CSR0 0x12u
#define RXCSR 0x16u
unsigned KbPortMilliseconds(unsigned high,unsigned low)
{
 unsigned i,remainder=0,quotient=0;
 /* Binary long division. Keep only the low quotient word; remainder is
  * bounded below 10000, so every intermediate fits in 32 bits. */
 for(i=0;i<32;++i){
  remainder=(remainder<<1)|(high>>31);high<<=1;
  if(remainder>=10000u)remainder-=10000u;
 }
 for(i=0;i<32;++i){
  remainder=(remainder<<1)|(low>>31);low<<=1;quotient<<=1;
  if(remainder>=10000u){remainder-=10000u;quotient|=1u;}
 }
 return quotient;
}
static int due(unsigned now,unsigned deadline){return (unsigned)(now-deadline)<0x80000000u;}
static int fail(KB_PORT *p)
{
 p->stage=KB_PORT_FAULT;p->session->enumeration.stage=KB_ERROR;KbPortCancelPending(p);
 p->release(p->client);return -1;
}
int KbPortInit(KB_PORT *p,KB_SESSION *s,const MT8167_USB20_IO *io,void *client,
 void (*delay)(void *,unsigned),int (*report)(void *,const unsigned char *),void (*release)(void *))
{
 KB_PORT next={0};
 if(!p || !s || !io || !io->read8 || !io->read16 || !io->write8 || !io->write16 || !delay || !report || !release)return -1;
 next.session=s;next.io=*io;next.client=client;next.delay=delay;next.report=report;next.release=release;next.stage=KB_PORT_WAIT;
 *p=next;s->enumeration.stage=KB_ERROR;return 0;
}
int KbPortFlush(KB_PORT *p)
{
 unsigned i,channel;unsigned short csr;
 if(!p || !p->session)return -1;
 /* MT8167 vendor musb_host.c: musb_h_ep0_flush_fifo (5 retries).
  * First stop REQPKT, then flush ready data, finally reset CSR0. */
 p->io.write8(p->io.context,INDEX,0);
 csr=p->io.read16(p->io.context,CSR0);
 p->io.write16(p->io.context,CSR0,(unsigned short)(csr&~0x20u));
 for(i=0;i<5;++i){
  if(!(p->io.read16(p->io.context,CSR0)&3u))break;
  p->io.write16(p->io.context,CSR0,0x100);p->delay(p->client,1);
 }
 if(p->io.read16(p->io.context,CSR0)&3u)return -1;
 p->io.write16(p->io.context,CSR0,0);
 for(channel=1;channel<=(A04_MUSB_BOOT_COMPOSITE_EXPERIMENT?2u:1u);++channel){
 p->io.write8(p->io.context,INDEX,(unsigned char)channel);
 /* Same source musb_h_flush_rxfifo: stop REQPKT/AUTOREQ/AUTOCLEAR,
  * FLUSHFIFO twice. Our RX1 is single-buffer PIO, no DMA. */
 csr=p->io.read16(p->io.context,RXCSR);
 if(csr&0x2000u)return -1; /* DMAENAB: ownership cannot be established here. */
 csr=(unsigned short)((csr|0x11u)&~(0x20u|0x4000u|0x8000u));
 p->io.write16(p->io.context,RXCSR,csr);p->io.write16(p->io.context,RXCSR,csr);
 p->io.write16(p->io.context,RXCSR,0x80); /* CLRDATATOG */
 if(p->io.read16(p->io.context,RXCSR)&0x21u)return -1;
 }
 p->io.write8(p->io.context,INDEX,0);return 0;
}
static unsigned interval(const KB_SESSION *s)
{
 unsigned n=s->enumeration.interval;
 if(s->speed!=0x40)return n;
 return ((1u<<(n-1))+7u)/8u; /* high-speed exponent in 125us units */
}
int KbPortEvents(KB_PORT *p,MT8167_USB20_EVENTS e,unsigned now)
{
 int r;EP0_STAGE old_stage;unsigned old_actual;KB_STAGE old_enum;
 if(!p || !p->session || p->stage==KB_PORT_FAULT)return -1;
 if(e.usb&0x80u)return fail(p);
 if(e.usb&0x20u){
  p->io.write8(p->io.context,POWER,(unsigned char)(p->io.read8(p->io.context,POWER)&~8u));
  p->session->enumeration.stage=KB_ERROR;KbPortCancelPending(p);p->release(p->client);
  if(KbPortFlush(p))return fail(p);
  p->stage=KB_PORT_WAIT;return 0;
 }
 if(e.usb&0x10u){
  /* Debounce before resetting. Ignore endpoint causes from the old device. */
  p->session->enumeration.stage=KB_ERROR;KbPortCancelPending(p);p->release(p->client);
  p->stage=KB_PORT_DEBOUNCE;p->deadline=now+100u;return 0;
 }
 if(p->stage==KB_PORT_ENUM && (e.tx&1u)){
  old_stage=p->session->ep0.transfer.stage;old_actual=p->session->ep0.transfer.actual;old_enum=p->session->enumeration.stage;
  r=KbSessionEp0Interrupt(p->session);if(r<0)return fail(p);
  if(r==1){p->stage=KB_PORT_READY;p->next_poll=now;
#if A04_MUSB_BOOT_COMPOSITE_EXPERIMENT
   p->secondary_next_poll=now;
#endif
   return 1;}
  if(old_stage!=p->session->ep0.transfer.stage || old_actual!=p->session->ep0.transfer.actual || old_enum!=p->session->enumeration.stage)p->deadline=now+2000u;
 }
 if(p->stage==KB_PORT_READY && p->rx_pending && (e.rx&2u)){
  r=KbSessionRxInterrupt(p->session);if(r<0)return fail(p);
  if(r==1 || r==2)p->rx_pending=0;
 }
#if A04_MUSB_BOOT_COMPOSITE_EXPERIMENT
 if(p->stage==KB_PORT_READY && p->secondary_rx_pending && (e.rx&4u)){
  r=KbSessionRxSecondary(p->session);if(r<0)return fail(p);
  if(r==1 || r==2)p->secondary_rx_pending=0;
 }
#endif
 return 0;
}
int KbPortTick(KB_PORT *p,unsigned now)
{
 unsigned char power,devctl;unsigned speed;
 if(!p || !p->session || p->stage==KB_PORT_FAULT)return -1;
 switch(p->stage){
 case KB_PORT_DEBOUNCE:
  if(!due(now,p->deadline))break;
  if(KbPortFlush(p))return fail(p);
  power=p->io.read8(p->io.context,POWER);
  p->io.write8(p->io.context,POWER,(unsigned char)(power|8u));p->stage=KB_PORT_RESET;p->deadline=now+50u;break;
 case KB_PORT_RESET:
  if(!due(now,p->deadline))break;
  power=p->io.read8(p->io.context,POWER);p->io.write8(p->io.context,POWER,(unsigned char)(power&~8u));
  p->stage=KB_PORT_RECOVER;p->deadline=now+10u;break;
 case KB_PORT_RECOVER:
  if(!due(now,p->deadline))break;
  power=p->io.read8(p->io.context,POWER);devctl=p->io.read8(p->io.context,DEVCTL);
  speed=(devctl&0x20u)?0xc0u:((power&0x10u)?0x40u:0x80u);
  if((devctl&0x80u) || !(devctl&4u) || KbSessionStart(p->session,&p->io,speed,p->client,p->delay,p->report))return fail(p);
  p->stage=KB_PORT_ENUM;p->deadline=now+2000u;break;
 case KB_PORT_ENUM:if(due(now,p->deadline))return fail(p);break;
 case KB_PORT_READY:
  /* Interrupt IN may NAK indefinitely while the keyboard is idle. Linux
   * musb_host.c keeps the request posted; bInterval is a polling cadence,
   * not a report deadline (HS interval 16 alone is 4096 ms). Disconnect,
   * STALL and protocol errors still terminate through KbPortEvents. */
  if(!p->rx_pending && due(now,p->next_poll)){
   if(KbSessionRequestReport(p->session))return fail(p);
   p->rx_pending=1;p->next_poll=now+interval(p->session);
  }
#if A04_MUSB_BOOT_COMPOSITE_EXPERIMENT
  if(p->session->enumeration.secondary_endpoint && !p->secondary_rx_pending && due(now,p->secondary_next_poll)){
   unsigned delay=p->session->enumeration.secondary_interval;
   if(KbSessionRequestSecondary(p->session))return fail(p);
   if(p->session->speed==0x40)delay=((1u<<(delay-1))+7u)/8u;
   p->secondary_rx_pending=1;p->secondary_next_poll=now+delay;
  }
#endif
  break;
 default:break;
 }
 return 0;
}
