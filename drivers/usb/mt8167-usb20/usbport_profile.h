/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_USBPORT_PROFILE_H
#define A04_USBPORT_PROFILE_H
/* RT81 endpoint construction 0x206c0 copies TT-context address to +0x30;
 * absent context is 0xffff. This is NOT the immediate external hub address.
 * Legacy no-TT zero remains accepted. Never turn a finite TT address into
 * a native transfer by discarding it, even for a control endpoint. */
static int a04_usbport_native_route(unsigned hub)
{return hub==0||hub==0xffff;}
/* RT81 0x206c0/0x22db0 normalizes HS bInterval to 1,2,4,8,16,32.
 * CT3 MUSB RX/TXINTERVAL instead expects exponent bInterval (1..16).
 * Only the observed RT81 range is invertible: never infer lost >32 periods. */
static int a04_usbport_interval(unsigned speed,unsigned type,unsigned period,unsigned *reg)
{
 unsigned shift;
 if(!reg||speed>2)return -1;
 *reg=0;if(type!=3)return 0;
 if(!period||period>255)return -1;
 if(speed!=2){*reg=period;return 0;}
 if(period>32||(period&(period-1)))return -1;
 for(shift=0;period>1;period>>=1)++shift;
 *reg=shift+1;return 0;
}
#endif
