#include "board_host.h"
#define DEVCTL 0x60
#define ULPI_DATA 0x74
static int valid(const A04_USB_BOARD *b)
{ return b && b->power && b->vbus && b->phy_read && b->phy_write && b->mac_read && b->mac_write && b->delay_ms && b->phy_read32; }
static void phy_update(const A04_USB_BOARD *b,unsigned reg,unsigned clear,unsigned set)
{ unsigned char v=b->phy_read(b->context,reg); b->phy_write(b->context,reg,(unsigned char)((v&~clear)|set)); }
/* MT8167 usb20_phy.c hs_slew_rate_cal: FRA=48, PARA=28, 1024 cycles.
 * Frequency-meter registers are relative to SIF+0x800, not the MAC. */
static int calibrate(const A04_USB_BOARD *b)
{
 unsigned i,data,value=4,x,remainder,numerator;
 b->phy_write(b->context,0x15,0x80);b->delay_ms(b->context,1);
 b->phy_write(b->context,0x711,1);b->phy_write(b->context,0x701,4);
 phy_update(b,0x703,0,1);
 for(i=0;i<3000;++i){if(b->phy_read(b->context,0x710)&1)break;b->delay_ms(b->context,1);}
 if(i<3000){
  data=b->phy_read32(b->context,0x70c);
  if(!data){value=8;}else {
   /* Match the Linux integer quotient without pulling ARM CRT division
    * exception helpers into a Windows 8.1 kernel driver. Numerator is small,
    * so the 32-bit remainder cannot overflow during long division. */
   numerator=1024u*48u*28u;remainder=0;x=0;
   for(i=0;i<32;++i){
    remainder=(remainder<<1)|(numerator>>31);numerator<<=1;x<<=1;
    if(remainder>=data){remainder-=data;x|=1u;}
   }
   value=(x+500u)/1000u;
  }
 }
 phy_update(b,0x703,1,0);phy_update(b,0x711,1,0);
 /* Linux's timeout fallback is 4; impossible measured values are failures. */
 b->phy_write(b->context,0x15,(unsigned char)((value<8?value:4u)<<4));
 phy_update(b,0x15,0x80,0);
 return value<8?0:-1;
}
void A04UsbHostStop(const A04_USB_BOARD *b)
{
 unsigned char v;
 if(!valid(b))return;
 v=b->mac_read(b->context,DEVCTL); b->mac_write(b->context,DEVCTL,(unsigned char)(v&~1u));
 v=b->mac_read(b->context,1);b->mac_write(b->context,1,(unsigned char)(v&~0x0cu));
 phy_update(b,0x6c,0x2e,0x10); phy_update(b,0x6d,0,0x3e);
 (void)b->vbus(b->context,0); (void)b->power(b->context,0);
}
int A04UsbHostPrepare(const A04_USB_BOARD *b)
{
 if(!valid(b) || b->power(b->context,1))return -1;
 /* Restore USB function and release forced suspend after the shared parent
  * has enabled VUSB and clocks. Offsets are relative to SIF+0x800. */
 b->delay_ms(b->context,1);
 phy_update(b,0x1d,0x10,0); phy_update(b,0x6b,0x04,0);
 phy_update(b,0x6e,0x01,0); phy_update(b,0x6a,0x04,0);
 phy_update(b,0x21,0x03,0); phy_update(b,0x68,0xf4,0);
 phy_update(b,0x69,0x3c,0); phy_update(b,0x6a,0xba,0);
 phy_update(b,0x1a,0x80,0x10); phy_update(b,0x18,0x08,0x06);
 b->delay_ms(b->context,1);
 if(calibrate(b)){A04UsbHostStop(b);return -1;}
 phy_update(b,0x6c,0x10,0x2e); phy_update(b,0x6d,0,0x3e);
 return 0;
}
int A04UsbHostRun(const A04_USB_BOARD *b)
{
 unsigned char v;unsigned attempt;
 if(!valid(b) || b->vbus(b->context,1))return -1;
 b->delay_ms(b->context,100);
 v=b->mac_read(b->context,DEVCTL); b->mac_write(b->context,DEVCTL,(unsigned char)(v&~1u));
 phy_update(b,0x6c,0x2e,0x10); phy_update(b,0x6d,0,0x3e);
 b->delay_ms(b->context,5);
 v=b->mac_read(b->context,ULPI_DATA); b->mac_write(b->context,ULPI_DATA,(unsigned char)((v|0x80u)&~0x40u));
 b->delay_ms(b->context,5);
 v=b->mac_read(b->context,DEVCTL); b->mac_write(b->context,DEVCTL,(unsigned char)(v|1u));
 phy_update(b,0x6c,0x10,0x2c); phy_update(b,0x6d,0,0x3e);
 /* The board Linux role driver polls BDEVICE clear for <=1s
  * after the existing PHY/session re-latch. A POWER write echo cannot prove
  * this transition. Require SESSION to remain latched, but do not require
  * HOSTMODE before CONNECT: that old prerequisite rejected an empty port.
  * These reads do not grant admission or force a different cable role. */
 for(attempt=0;attempt<=200;++attempt) {
  v=b->mac_read(b->context,DEVCTL);
  if((v&0x81u)==1u)break;
  if(attempt==200)return A04_USB_HOST_TRANSITION_TIMEOUT;
  b->delay_ms(b->context,5);
 }
 /* musb_core.c musb_start: enable suspend signalling and HS negotiation.
  * Do not retain a firmware RESET/SUSPEND/RESUME or device SOFTCONN. */
 b->mac_write(b->context,1,0x21);
 /* HSMODE (bit4) is read-only and may already be set. A stuck RESET,
  * SUSPENDM, RESUME or device SOFTCONN must not count as host readiness. */
 return (b->mac_read(b->context,1)&0x6fu)==0x21u ? 0:A04_USB_HOST_POWER_STATE;
}
int A04UsbHostHealth(const A04_USB_BOARD *b,unsigned *devctl)
{
 if(!valid(b) || !devctl)return -1;
 *devctl=b->mac_read(b->context,DEVCTL);
 return (*devctl&0x81u)==1u ? 0:-1;
}
int A04UsbHostStart(const A04_USB_BOARD *b)
{
 if(A04UsbHostPrepare(b))return -1;
 if(A04UsbHostRun(b)){A04UsbHostStop(b);return -1;}
 return 0;
}
