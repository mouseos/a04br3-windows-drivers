#include "controller.h"
#define INDEX 0x0e
#define TXFIFOSZ 0x62
#define RXFIFOSZ 0x63
#define TXFIFOADD 0x64
#define RXFIFOADD 0x66
#define INTRTX 0x02
#define INTRRX 0x04
#define INTRTXE 0x06
#define INTRRXE 0x08
#define INTRUSB 0x0a
#define INTRUSBE 0x0b
#define L1_STATUS 0xa0
#define L1_MASK 0xa4
#define L1_USB (1u|2u|4u)
#define USB_EVENTS (0x10u|0x20u|0x80u)
static int valid(const MT8167_USB20_CONTROLLER *c)
{ return c && c->io.read8 && c->io.read16 && c->io.write8 && c->io.write16 && c->read32 && c->write32; }
int Mt8167UsbFifoInit(const MT8167_USB20_CONTROLLER *c,unsigned bytes)
{
 void *p; if(!valid(c)||bytes<128) return -1; p=c->io.context;
 /* 64-byte shared EP0 at offset 0, 64-byte EP1 receive at offset 64. */
 c->io.write8(p,INDEX,0); c->io.write8(p,TXFIFOSZ,3); c->io.write16(p,TXFIFOADD,0);
 c->io.write8(p,RXFIFOSZ,3); c->io.write16(p,RXFIFOADD,0);
 c->io.write8(p,INDEX,1); c->io.write8(p,RXFIFOSZ,3); c->io.write16(p,RXFIFOADD,8);
 c->io.write8(p,INDEX,0); return 0;
}
int Mt8167UsbInterruptEnable(const MT8167_USB20_CONTROLLER *c)
{
 void *p; unsigned m; if(!valid(c))return -1; p=c->io.context;
 c->io.write16(p,INTRTXE,1); c->io.write16(p,INTRRXE,2); c->io.write8(p,INTRUSBE,USB_EVENTS);
 m=c->read32(p,L1_MASK); c->write32(p,L1_MASK,m|L1_USB); return 0;
}
void Mt8167UsbInterruptDisable(const MT8167_USB20_CONTROLLER *c)
{
 void *p; unsigned m; if(!valid(c))return; p=c->io.context;
 m=c->read32(p,L1_MASK); c->write32(p,L1_MASK,m&~L1_USB);
 c->io.write16(p,INTRTXE,0); c->io.write16(p,INTRRXE,0); c->io.write8(p,INTRUSBE,0);
}
int Mt8167UsbInterruptCapture(const MT8167_USB20_CONTROLLER *c,MT8167_USB20_EVENTS *e)
{
 void *p; unsigned l1; if(!valid(c)||!e)return -1; p=c->io.context;
 e->usb=e->tx=e->rx=0; l1=c->read32(p,L1_STATUS)&c->read32(p,L1_MASK)&L1_USB;
 if(!l1)return 0;
 if(l1&4u)e->usb=c->io.read8(p,INTRUSB)&c->io.read8(p,INTRUSBE);
 if(l1&1u)e->tx=c->io.read16(p,INTRTX)&c->io.read16(p,INTRTXE);
 if(l1&2u)e->rx=c->io.read16(p,INTRRX)&c->io.read16(p,INTRRXE);
 if(e->rx)c->io.write16(p,INTRRX,(unsigned short)e->rx);
 if(e->tx)c->io.write16(p,INTRTX,(unsigned short)e->tx);
 if(e->usb)c->io.write8(p,INTRUSB,(unsigned char)e->usb);
 return (e->usb||e->tx||e->rx)?1:0;
}
