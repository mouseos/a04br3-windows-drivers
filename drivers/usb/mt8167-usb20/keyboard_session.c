#include "keyboard_session.h"
#include "keyboard_rx.h"
#define INDEX 0x0e
#define TYPE0 0x1a
#define NAKLIMIT0 0x1b
#define TXFUNCADDR0 0x480
#define TXHUBADDR0 0x482
static int failed(KB_SESSION *s){s->enumeration.stage=KB_ERROR;return -1;}
static int submit(KB_SESSION *s)
{
 KB_CONTROL c;
 if(KbNextControl(&s->enumeration,&c))return failed(s);
 s->io.write8(s->io.context,INDEX,0);
 s->io.write16(s->io.context,TXFUNCADDR0,(unsigned short)c.address);
 s->io.write16(s->io.context,TXHUBADDR0,0);
 s->io.write8(s->io.context,TYPE0,(unsigned char)s->speed);
 s->io.write8(s->io.context,NAKLIMIT0,0);
 return Mt8167Ep0Start(&s->ep0,&s->io,c.setup,s->buffer,c.length,c.max_packet0)==EP0_SEND_SETUP?0:failed(s);
}
int KbSessionStart(KB_SESSION *s,const MT8167_USB20_IO *io,unsigned speed,void *client,void (*delay)(void *,unsigned),int (*report)(void *,const unsigned char *))
{
 if(!s || !io || !io->read8 || !io->read16 || !io->write8 || !io->write16 || !delay || !report || (speed!=0x40 && speed!=0x80 && speed!=0xc0))return -1;
 s->io=*io; s->speed=speed; s->client=client; s->delay_ms=delay; s->report=report;
 KbEnumStart(&s->enumeration);
 /* Linux hub_port_init: high-speed EP0 is 64 bytes, low-speed is 8.
  * Full-speed learns one of 8/16/32/64 from the initial descriptor. */
 if(speed==0x40)s->enumeration.max_packet0=64;
 return submit(s);
}
int KbSessionEp0Interrupt(KB_SESSION *s)
{
 EP0_ACTION a; KB_STAGE prior;
 if(!s)return -1;
 a=Mt8167Ep0Interrupt(&s->ep0);
 if(a==EP0_ABORT)return failed(s);
 if(a!=EP0_COMPLETE)return 0;
 prior=s->enumeration.stage;
 if(KbControlComplete(&s->enumeration,s->buffer,s->ep0.transfer.actual))return failed(s);
 if(prior==KB_DEVICE8 && ((s->speed==0xc0 && s->enumeration.max_packet0!=8) ||
    (s->speed==0x40 && s->enumeration.max_packet0!=64)))return failed(s);
 if(prior==KB_ADDRESS)s->delay_ms(s->client,2);
 if(s->enumeration.stage!=KB_READY)return submit(s);
 if(KbRxConfigureBoot(&s->io,1,s->enumeration.endpoint,s->enumeration.endpoint_packet,s->enumeration.interval,s->speed,s->enumeration.protocol))return failed(s);
 return 1;
}
int KbSessionRequestReport(KB_SESSION *s)
{ if(!s || s->enumeration.stage!=KB_READY)return -1; return KbRxRequest(&s->io)?failed(s):0; }
int KbSessionRxInterrupt(KB_SESSION *s)
{
 unsigned char report[8]; int result;
 if(!s || s->enumeration.stage!=KB_READY)return -1;
 result=KbRxCompleteBoot(&s->io,report,s->enumeration.protocol,s->enumeration.endpoint_packet); if(result<0)return failed(s);if(result!=1)return result;
 return s->report(s->client,report)?failed(s):1;
}
