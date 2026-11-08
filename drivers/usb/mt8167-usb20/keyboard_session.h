#ifndef MT8167_USB20_KEYBOARD_SESSION_H
#define MT8167_USB20_KEYBOARD_SESSION_H
#include "keyboard.h"
#include "ep0_pio.h"
typedef struct {
 MT8167_USB20_IO io; MT8167_USB20_EP0 ep0; KB_ENUM enumeration;
 unsigned speed;
#if A04_MUSB_BOOT_COMPOSITE_EXPERIMENT
 unsigned report_protocol;
#endif
 unsigned char buffer[A04_USB_CONFIG_MAX];
 void *client; void (*delay_ms)(void *,unsigned); int (*report)(void *,const unsigned char *);
} KB_SESSION;
/* Caller initializes host, port reset, EP0 and endpoint 1 RX FIFO/IRQ.
 * Call only at PASSIVE_LEVEL under a single controller lock. */
int KbSessionStart(KB_SESSION *,const MT8167_USB20_IO *,unsigned,void *,void (*)(void *,unsigned),int (*)(void *,const unsigned char *));
int KbSessionEp0Interrupt(KB_SESSION *); /* 0=pending, 1=boot interface ready, -1=error */
int KbSessionRequestReport(KB_SESSION *); /* schedule no faster than bInterval */
int KbSessionRxInterrupt(KB_SESSION *); /* 0=pending, 1=delivered, 2=NAK, -1=error */
static inline unsigned KbSessionReportProtocol(const KB_SESSION *s)
{
#if A04_MUSB_BOOT_COMPOSITE_EXPERIMENT
 return s->report_protocol;
#else
 return s->enumeration.protocol;
#endif
}
static inline int KbSessionHasMouse(const KB_SESSION *s)
{
 if(s->enumeration.protocol==A04_BOOT_MOUSE)return 1;
#if A04_MUSB_BOOT_COMPOSITE_EXPERIMENT
 return s->enumeration.secondary_endpoint!=0;
#else
 return 0;
#endif
}
int KbSessionRequestSecondary(KB_SESSION *);
int KbSessionRxSecondary(KB_SESSION *);
#endif
