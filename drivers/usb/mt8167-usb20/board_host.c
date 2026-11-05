#include "board_host.h"
#define DEVCTL 0x60
#define ULPI_DATA 0x74
static int valid(const A04_USB_BOARD *b)
{ return b && b->power && b->vbus && b->phy_read && b->phy_write && b->mac_read && b->mac_write && b->delay_ms; }
static void phy_update(const A04_USB_BOARD *b,unsigned reg,unsigned clear,unsigned set)
{ unsigned char v=b->phy_read(b->context,reg); b->phy_write(b->context,reg,(unsigned char)((v&~clear)|set)); }
void A04UsbHostStop(const A04_USB_BOARD *b)
{
 unsigned char v;
 if(!valid(b))return;
 v=b->mac_read(b->context,DEVCTL); b->mac_write(b->context,DEVCTL,(unsigned char)(v&~1u));
 phy_update(b,0x6c,0x2e,0x10); phy_update(b,0x6d,0,0x3e);
 (void)b->vbus(b->context,0); (void)b->power(b->context,0);
}
int A04UsbHostStart(const A04_USB_BOARD *b)
{
 unsigned char v;
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
 phy_update(b,0x6c,0x10,0x2e); phy_update(b,0x6d,0,0x3e);
 if(b->vbus(b->context,1)) { A04UsbHostStop(b); return -1; }
 b->delay_ms(b->context,100);
 v=b->mac_read(b->context,DEVCTL); b->mac_write(b->context,DEVCTL,(unsigned char)(v&~1u));
 phy_update(b,0x6c,0x2e,0x10); phy_update(b,0x6d,0,0x3e);
 b->delay_ms(b->context,5);
 v=b->mac_read(b->context,ULPI_DATA); b->mac_write(b->context,ULPI_DATA,(unsigned char)((v|0x80u)&~0x40u));
 b->delay_ms(b->context,5);
 v=b->mac_read(b->context,DEVCTL); b->mac_write(b->context,DEVCTL,(unsigned char)(v|1u));
 phy_update(b,0x6c,0x10,0x2c); phy_update(b,0x6d,0,0x3e);
 return 0;
}
