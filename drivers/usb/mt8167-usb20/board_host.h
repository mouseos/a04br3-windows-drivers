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
 unsigned (*phy_read32)(void *,unsigned);
} A04_USB_BOARD;
int A04UsbHostStart(const A04_USB_BOARD *);
/* Prepare owns power and initializes the PHY. Arm the IRQ before Run so a
 * device already connected when SESSION starts cannot lose CONNECT. */
int A04UsbHostPrepare(const A04_USB_BOARD *);
/* Run reports actual MAC convergence, not connector voltage or attachment.
 * On failure the caller must first disarm/join IRQ work, then call Stop. */
#define A04_USB_HOST_TRANSITION_TIMEOUT (-2)
#define A04_USB_HOST_POWER_STATE (-3)
int A04UsbHostRun(const A04_USB_BOARD *);
/* Read-only invariant while the caller holds the live board lease. Neither
 * HOSTMODE nor VBUSVALID are required; reset is a legitimate port operation. */
int A04UsbHostHealth(const A04_USB_BOARD *,unsigned *devctl);
void A04UsbHostStop(const A04_USB_BOARD *);
/* connect/disconnect is reported by MUSB INTRUSB, not by DEVCTL alone. */
#endif
