#ifndef MT8167_USB20_BOARD_HOST_H
#define MT8167_USB20_BOARD_HOST_H
/* board owns clocks, regulator and GPIO22; callbacks must coordinate shared
 * clock and power ownership and return zero only after success. */
typedef struct {
 void *context;
 int (*power)(void *,int);
 int (*vbus)(void *,int);
 unsigned char (*phy_read)(void *,unsigned);
 void (*phy_write)(void *,unsigned,unsigned char);
 unsigned char (*mac_read)(void *,unsigned);
 void (*mac_write)(void *,unsigned,unsigned char);
 void (*delay_ms)(void *,unsigned);
} A04_USB_BOARD;
int A04UsbHostStart(const A04_USB_BOARD *);
void A04UsbHostStop(const A04_USB_BOARD *);
/* connect/disconnect is reported by MUSB INTRUSB, not by DEVCTL alone. */
#endif
