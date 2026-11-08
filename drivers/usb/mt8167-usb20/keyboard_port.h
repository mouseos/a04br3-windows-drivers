/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_KEYBOARD_PORT_H
#define MT8167_KEYBOARD_PORT_H
#include "keyboard_session.h"
#include "controller.h"
typedef enum { KB_PORT_WAIT, KB_PORT_DEBOUNCE, KB_PORT_RESET,
 KB_PORT_RECOVER, KB_PORT_ENUM, KB_PORT_READY, KB_PORT_FAULT } KB_PORT_STAGE;
typedef struct {
 KB_SESSION *session; MT8167_USB20_IO io; KB_PORT_STAGE stage;
 unsigned deadline, next_poll, rx_deadline; int rx_pending;
#if A04_MUSB_BOOT_COMPOSITE_EXPERIMENT
 unsigned secondary_next_poll;int secondary_rx_pending;
#endif
 void *client; void (*delay)(void *,unsigned);
 int (*report)(void *,const unsigned char *); void (*release)(void *);
} KB_PORT;
/* Caller owns powered host-mode MAC/PHY, FIFO/IRQs and serializes INDEX.
 * now is monotonic milliseconds modulo 2^32; tick at least once per second.
 * On -1 the caller must mask IRQs and stop the session/VBUS before reuse. */
/* 100ns counter -> milliseconds modulo 2^32, without ARM CRT 64bit divide. */
unsigned KbPortMilliseconds(unsigned high,unsigned low);
int KbPortInit(KB_PORT *,KB_SESSION *,const MT8167_USB20_IO *,void *,
 void (*)(void *,unsigned),int (*)(void *,const unsigned char *),void (*)(void *));
int KbPortEvents(KB_PORT *,MT8167_USB20_EVENTS,unsigned now);
int KbPortTick(KB_PORT *,unsigned now);
/* Stop requests and clear owned PIO FIFO state after disconnect/reset.
 * No DMA cancellation is implied. Must never run with an active DMA engine. */
int KbPortFlush(KB_PORT *);
static inline void KbPortCancelPending(KB_PORT *p)
{
 p->rx_pending=0;
#if A04_MUSB_BOOT_COMPOSITE_EXPERIMENT
 p->secondary_rx_pending=0;
#endif
}
#endif
