/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MUSB_HOST_CONTROL_H
#define A04_MUSB_HOST_CONTROL_H
#include "host_core.h"
#if A04_MUSB_HOST_CORE_EXPERIMENT
#include "controller.h"
/* Direct-device EP0 only. Caller owns controller/INDEX lock, port power,
 * reset, request memory, buffer, interrupt arm and deadline. No DMA. */
struct a04_control_request {
 struct a04_transfer transfer; /* first: queue element is request address */
 unsigned char setup[8], *buffer;
 unsigned address, speed, max_packet;
};
struct a04_control_host {
 struct a04_root_port port;
 struct a04_host_queue queue;
 MT8167_USB20_EP0 ep0;
 MT8167_USB20_IO io;
 struct a04_control_request *active;
};
int a04_control_init(struct a04_control_host *,const MT8167_USB20_IO *);
int a04_control_submit(struct a04_control_host *,struct a04_control_request *);
/* Starts at most one request; completion must be consumed before next start. */
int a04_control_start(struct a04_control_host *);
/* Deliver acknowledged events from Mt8167UsbInterruptCapture or WDF worker.
 * Returns one completed request on success; abort requires quiesce. */
int a04_control_events(struct a04_control_host *,MT8167_USB20_EVENTS,struct a04_control_request **);
int a04_control_cancel(struct a04_control_host *,struct a04_control_request *);
/* Call only after endpoint is stopped and stale IRQs drained under IRQ lock. */
int a04_control_quiesced(struct a04_control_host *,struct a04_control_request **);
#endif
#endif
