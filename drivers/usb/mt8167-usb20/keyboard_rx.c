#include "keyboard_rx.h"
#define INDEX 0x0e
#define RXMAXP 0x14
#define RXCSR 0x16
#define RXCOUNT 0x18
#define RXTYPE 0x1c
#define RXINTERVAL 0x1d
#define FIFO1 0x24
#define RXFUNCADDR1 0x48c
#define RXHUBADDR1 0x48e
#define RX_READY 0x01
#define RX_ERROR 0x04
#define RX_NAK 0x08
#define RX_REQUEST 0x20
#define RX_STALL 0x40
#define RX_CLEAR_TOGGLE 0x80
#define RX_WZC (RX_READY | RX_ERROR | RX_NAK | RX_STALL)
static int valid(const MT8167_USB20_IO *io)
{ return io && io->read8 && io->read16 && io->write8 && io->write16; }
int KbRxConfigure(const MT8167_USB20_IO *io,unsigned address,unsigned endpoint,unsigned max_packet,unsigned interval,unsigned speed)
{
 if(!valid(io) || !address || address>127 || !endpoint || endpoint>15 || max_packet<8 || max_packet>64 || !interval || interval>255 || (speed!=0x40 && speed!=0x80 && speed!=0xc0) || (speed==0xc0 && max_packet>8) || (speed==0x40 && interval>16)) return -1;
 io->write8(io->context,INDEX,1);
 io->write16(io->context,RXFUNCADDR1,(unsigned short)address);
 io->write16(io->context,RXHUBADDR1,0);
 io->write8(io->context,RXTYPE,(unsigned char)(speed | 0x30 | endpoint));
 io->write8(io->context,RXINTERVAL,(unsigned char)interval);
 io->write16(io->context,RXMAXP,(unsigned short)max_packet);
 io->write16(io->context,RXCSR,RX_CLEAR_TOGGLE);
 return 0;
}
int KbRxRequest(const MT8167_USB20_IO *io)
{
 unsigned short csr;
 if(!valid(io))return -1;
 io->write8(io->context,INDEX,1); csr=io->read16(io->context,RXCSR);
 if(csr & (RX_READY | RX_REQUEST | RX_ERROR | RX_STALL))return -1;
 io->write16(io->context,RXCSR,(unsigned short)((csr | RX_WZC | RX_REQUEST) & ~RX_NAK));
 return 0;
}
int KbRxComplete(const MT8167_USB20_IO *io,unsigned char report[8])
{
 unsigned short csr,count; unsigned i;
 if(!valid(io) || !report)return -1;
 io->write8(io->context,INDEX,1); csr=io->read16(io->context,RXCSR);
 if(csr & (RX_ERROR | RX_STALL))return -1;
 if(csr & RX_NAK) { io->write16(io->context,RXCSR,(unsigned short)((csr | RX_WZC) & ~(RX_NAK | RX_REQUEST))); return 0; }
 if(!(csr & RX_READY))return 0;
 count=io->read16(io->context,RXCOUNT);
 if(count!=8)return -1;
 for(i=0;i<8;i++)report[i]=io->read8(io->context,FIFO1);
 io->write16(io->context,RXCSR,(unsigned short)((csr | RX_WZC) & ~(RX_READY | RX_REQUEST)));
 return 1;
}
