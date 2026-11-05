#include "gpio22.h"
#define GPIO22_BIT (1u<<6)
#define MODE22 0x340u
#define DIR1 0x10u
#define DOUT1 0x110u
static int valid(const A04_GPIO_IO *io){return io && io->read32 && io->write32;}
int A04Gpio22Prepare(const A04_GPIO_IO *io)
{
 void *p; if(!valid(io))return -1; p=io->context;
 /* Check, do not alter the shared mux register. Set output low before output. */
 if(io->read32(p,MODE22)&(7u<<6))return -1;
 io->write32(p,DOUT1+8,GPIO22_BIT);
 io->write32(p,DIR1+4,GPIO22_BIT);
 if(!(io->read32(p,DIR1)&GPIO22_BIT) || (io->read32(p,DOUT1)&GPIO22_BIT))return -1;
 return 0;
}
int A04Gpio22Vbus(const A04_GPIO_IO *io,int on)
{
 void *p; unsigned v; if(!valid(io))return -1; p=io->context;
 if((io->read32(p,MODE22)&(7u<<6)) || !(io->read32(p,DIR1)&GPIO22_BIT))return -1;
 io->write32(p,DOUT1+(on?4u:8u),GPIO22_BIT); v=io->read32(p,DOUT1);
 return (!!(v&GPIO22_BIT)==!!on)?0:-1;
}
