#ifndef MT8167_USB20_KEYBOARD_SESSION_H
#define MT8167_USB20_KEYBOARD_SESSION_H
#include "keyboard.h"
#include "ep0_pio.h"
typedef struct {
 MT8167_USB20_IO io; MT8167_USB20_EP0 ep0; KB_ENUM enumeration;
 unsigned speed; unsigned char buffer[A04_USB_CONFIG_MAX];
 void *client; void (*delay_ms)(void *,unsigned); int (*report)(void *,const unsigned char *);
} KB_SESSION;
/* Caller initializes host, port reset, EP0 and endpoint 1 RX FIFO/IRQ.
 * Call only at PASSIVE_LEVEL under a single controller lock. */
int KbSessionStart(KB_SESSION *,const MT8167_USB20_IO *,unsigned,void *,void (*)(void *,unsigned),int (*)(void *,const unsigned char *));
int KbSessionEp0Interrupt(KB_SESSION *); /* 0=pending, 1=boot interface ready, -1=error */
int KbSessionRequestReport(KB_SESSION *); /* schedule no faster than bInterval */
int KbSessionRxInterrupt(KB_SESSION *); /* 0=pending, 1=delivered, 2=NAK, -1=error */
#endif
