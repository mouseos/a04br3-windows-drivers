/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Board sequence derived from xnu_kernel_benesse_a04br3 A04BR3SDIO.cpp start(),
 * A04BR3SDIOPads.h. It does not implement ownership or claim resources. */
#include "board_sequence.h"
static int ready(const struct a04_wifi_board_io *io)
{ return io->qualified(io->context)==1; }
static int read_checked(const struct a04_wifi_board_io *io,enum a04_wifi_board_bank b,unsigned off,uint32_t *v)
{ return !ready(io) || io->read(io->context,b,off,v) || *v==UINT32_MAX ? -1:0; }
static int write_checked(const struct a04_wifi_board_io *io,enum a04_wifi_board_bank b,unsigned off,uint32_t v)
{ return !ready(io) || io->write(io->context,b,off,v) ? -1:0; }
static int delay_checked(const struct a04_wifi_board_io *io,unsigned ms)
{ return !ready(io) || io->delay_ms(io->context,ms) || !ready(io) ? -1:0; }
static int update(const struct a04_wifi_board_io *io,enum a04_wifi_board_bank bank,unsigned offset,uint32_t clear,uint32_t set)
{uint32_t value;
 if(bank!=A04_WIFI_MSDC)return !ready(io)||!io->update_shared||io->update_shared(io->context,bank,offset,clear,set)?-1:0;
 if(read_checked(io,bank,offset,&value))return -1;
 return write_checked(io,bank,offset,(value&~clear)|set);}
static int pin(const struct a04_wifi_board_io *io,unsigned p,unsigned mode)
{unsigned shift=(p%5)*3;return update(io,A04_WIFI_GPIO,0x300+(p/5)*16,7u<<shift,mode<<shift);}
static int output(const struct a04_wifi_board_io *io,unsigned p,int high)
{return pin(io,p,0)||write_checked(io,A04_WIFI_GPIO,(high?0x104:0x108)+(p/32)*16,1u<<(p%32))||write_checked(io,A04_WIFI_GPIO,4+(p/32)*16,1u<<(p%32));}
static int wait_reg(const struct a04_wifi_board_io *io,unsigned offset,uint32_t mask,uint32_t wanted)
{unsigned i;uint32_t value;for(i=0;i<100;i++){if(read_checked(io,A04_WIFI_MSDC,offset,&value))return -1;if((value&mask)==wanted)return 0;if(delay_checked(io,1))return -1;}return -1;}
int a04_wifi_board_start(struct a04_wifi_board *s,const struct a04_wifi_board_io *io)
{
 unsigned p;
 if(!s||!io||!io->qualified||!io->read||!io->write||!io->delay_ms||!io->update_shared||s->started||s->poisoned||!ready(io))return -1;
 s->started=1;
#define WRITE(bank,off,val) do{if(write_checked(io,bank,off,val))goto fail;}while(0)
#define DELAY(ms) do{if(delay_checked(io,ms))goto fail;}while(0)
 WRITE(A04_WIFI_TOP,0xa0,2);if(update(io,A04_WIFI_TOP,0x40,7u<<3,0))goto fail;
 WRITE(A04_WIFI_TOP,0x9c,1);WRITE(A04_WIFI_TOP,0x6c,1u<<30);WRITE(A04_WIFI_TOP,0xb0,2);
 if(output(io,4,0)||output(io,7,0))goto fail;
 DELAY(10);
 if(output(io,4,1))goto fail;
 DELAY(30);
 if(output(io,7,1))goto fail;
 DELAY(30);
 for(p=68;p<=73;p++)if(pin(io,p,1))goto fail;
 WRITE(A04_WIFI_GPIO,0xe48,(1u<<6)|(1u<<5)|(1u<<10)|(1u<<9)|(1u<<14)|(1u<<13));
 WRITE(A04_WIFI_GPIO,0xe44,(1u<<4)|(1u<<8)|(1u<<12));
 WRITE(A04_WIFI_GPIO,0xe58,(1u<<10)|(1u<<9)|(1u<<2)|(1u<<1)|(1u<<4));
 WRITE(A04_WIFI_GPIO,0xe54,(1u<<8)|1u|(1u<<6)|(1u<<5));
 WRITE(A04_WIFI_GPIO,0x934,0x7e);WRITE(A04_WIFI_MSDC,0x10,0);
 if(update(io,A04_WIFI_MSDC,0,0,4)||wait_reg(io,0,4,0))goto fail;
 if(update(io,A04_WIFI_MSDC,0,(3u<<20)|(0xfffu<<8)|2u,9u|(17u<<8))||wait_reg(io,0,0x80,0x80))goto fail;
 if(update(io,A04_WIFI_MSDC,0x30,3u<<16,(1u<<19)|(0xffu<<24)))goto fail;
 DELAY(10);if(!ready(io))goto fail;s->ready=1;return 0;
fail:s->poisoned=1;return -1;
#undef WRITE
#undef DELAY
}
