/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "pio_transfer.h"
#define INDEX 0x0e
#define TXCSR 0x12
#define RXCSR 0x16
#define FIFO 0x24
static int valid(const MT8167_USB20_IO *io)
{return io&&io->read8&&io->read16&&io->write8&&io->write16;}
static int issue(const MT8167_USB20_IO *io,struct a04_pio_transfer *t)
{
 unsigned i,left=t->bytes-t->actual;
 io->write8(io->context,INDEX,t->lane);
 if(!t->ep.direction){io->write16(io->context,RXCSR,0x20);return 0;}
 t->pending=left<t->ep.packet?left:t->ep.packet;
 for(i=0;i<t->pending;i++)io->write8(io->context,(FIFO-4+4*t->lane),t->buffer[t->actual+i]);
 io->write16(io->context,TXCSR,0x2001);return 0;
}
int a04_pio_configure_lane(const MT8167_USB20_IO *io,struct a04_pio_transfer *t,
 const struct a04_pio_endpoint *e,unsigned char *buffer,unsigned bytes,unsigned lane)
{
 unsigned type,base;
 if(!valid(io)||!t||!e||lane<1||lane>MT_MUSB_DATA_LANES||!e->address||e->address>127||!e->endpoint||e->endpoint>15||
  !e->packet||e->packet>512||e->direction>1||e->toggle>1||
  (e->speed!=0x40&&e->speed!=0x80&&e->speed!=0xc0)||
  (e->type!=2&&e->type!=3)||(e->speed==0xc0&&(e->type!=3||e->packet>8))||
  (e->type==3&&(!e->period||e->period>255))||(bytes&&!buffer))return -1;
 /* Direct root-port bulk: FS legal maxpacket values or HS exactly 512.
  * No TT/split, ISO, high-bandwidth interrupt or DMA is introduced. */
 if(e->type==3&&e->packet>64)return -1;
 if(e->type==2&&!((e->speed==0x40&&e->packet==512)||
   (e->speed==0x80&&(e->packet==8||e->packet==16||e->packet==32||e->packet==64))))return -1;
 /* CT3 fifo_setup_for_host: log2(bytes)-3, offset>>3. Each lane owns
  * distinct TX/RX512 banks, under the single PASSIVE MMIO owner. */
 io->write8(io->context,INDEX,lane);
 if((io->read16(io->context,TXCSR)&3)||(io->read16(io->context,RXCSR)&0x21))return -1;
 io->write8(io->context,0x62,6);io->write8(io->context,0x63,6);
 base=MT_MUSB_FIFO_BASE(lane);
 io->write16(io->context,0x64,(unsigned short)(base>>3));io->write16(io->context,0x66,(unsigned short)((base+512)>>3));
 /* Never issue a packet when bank writes did not latch. Caller must use
  * its ordinary flush/StopAndProve path, not report a successful transfer. */
 if(io->read8(io->context,0x62)!=6||io->read8(io->context,0x63)!=6||
   io->read16(io->context,0x64)!=(base>>3)||io->read16(io->context,0x66)!=((base+512)>>3))return -1;
 t->lane=lane;t->ep=*e;t->buffer=buffer;t->bytes=bytes;t->actual=0;t->pending=0;t->zlp=bytes==0;t->active=1;
 io->write8(io->context,INDEX,t->lane);type=e->speed|(e->type<<4)|e->endpoint;
 if(e->direction){
  io->write8(io->context,(0x480+8*lane),(unsigned char)e->address);io->write16(io->context,(0x482+8*lane),0);
  io->write16(io->context,0x10,(unsigned short)e->packet);io->write8(io->context,0x1a,(unsigned char)type);
  io->write8(io->context,0x1b,(unsigned char)(e->type==3?e->period:0));
  io->write16(io->context,TXCSR,(unsigned short)(0x2208|(e->toggle?0x100:0)));
  io->write16(io->context,TXCSR,(unsigned short)(0x2200|(e->toggle?0x100:0)));
 }else{
  io->write8(io->context,(0x484+8*lane),(unsigned char)e->address);io->write16(io->context,(0x486+8*lane),0);
  io->write16(io->context,0x14,(unsigned short)e->packet);io->write8(io->context,0x1c,(unsigned char)type);
  io->write8(io->context,0x1d,(unsigned char)(e->type==3?e->period:0));
  io->write16(io->context,RXCSR,(unsigned short)(0x410|(e->toggle?0x200:0)));
  io->write16(io->context,RXCSR,(unsigned short)(0x400|(e->toggle?0x200:0)));
 }
 return issue(io,t);
}
int a04_pio_configure(const MT8167_USB20_IO *io,struct a04_pio_transfer *t,
 const struct a04_pio_endpoint *e,unsigned char *buffer,unsigned bytes)
{return a04_pio_configure_lane(io,t,e,buffer,bytes,1);}
int a04_pio_abort(const MT8167_USB20_IO *io,struct a04_pio_transfer *t)
{
 unsigned csr;
 if(!valid(io)||!t)return -1;
 io->write8(io->context,INDEX,t->lane);
 if(t->ep.direction){
  csr=io->read16(io->context,TXCSR);t->ep.toggle=!!(csr&0x100);
  io->write16(io->context,TXCSR,0x2008);io->write16(io->context,TXCSR,0x2008);
  if(io->read16(io->context,TXCSR)&3)return -1;
 }else{
  csr=io->read16(io->context,RXCSR);t->ep.toggle=!!(csr&0x200);
  io->write16(io->context,RXCSR,0x10);io->write16(io->context,RXCSR,0x10);
  if(io->read16(io->context,RXCSR)&0x21)return -1;
 }
 t->active=0;return 0;
}
int a04_pio_event(const MT8167_USB20_IO *io,struct a04_pio_transfer *t)
{
 unsigned csr,n,i;
 if(!valid(io)||!t||!t->active)return -1;
 io->write8(io->context,INDEX,t->lane);
 csr=io->read16(io->context,t->ep.direction?TXCSR:RXCSR);
 if(csr&(t->ep.direction?0x20:0x40))return A04_PIO_STALL;
 if(csr&4)return A04_PIO_ERROR;
 if(csr&(t->ep.direction?0x80:8)){
  if(t->ep.direction){
   io->write16(io->context,TXCSR,(unsigned short)((csr|0xa6)&~0x80u));
   /* A NAKed OUT packet is still in FIFO; retry it without duplicating bytes. */
   io->write16(io->context,TXCSR,(unsigned short)(((csr|0x2001|0x26)&~0x80u)));
  }else{
   io->write16(io->context,RXCSR,(unsigned short)((csr|0x4d)&~0x28u));
   io->write16(io->context,RXCSR,0x20);
  }
  return 0;
 }
 if(t->ep.direction){
  if(csr&3)return 0;
  t->actual+=t->pending;t->ep.toggle=!!(csr&0x100);
  if(t->actual==t->bytes){t->active=0;return 1;}
 }else{
  if(!(csr&1))return 0;
  n=io->read16(io->context,0x18);
  if(n>t->ep.packet||n>t->bytes-t->actual)return -1;
  for(i=0;i<n;i++)t->buffer[t->actual+i]=io->read8(io->context,(FIFO-4+4*t->lane));
  t->actual+=n;t->ep.toggle=!!(csr&0x200);
  io->write16(io->context,RXCSR,(unsigned short)((csr|0x4d)&~0x21u));
  if(n<t->ep.packet||t->actual==t->bytes){t->active=0;return 1;}
 }
 return issue(io,t);
}
