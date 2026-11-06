/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MUSB_HOST_CORE_H
#define A04_MUSB_HOST_CORE_H
#ifndef A04_MUSB_HOST_CORE_EXPERIMENT
#define A04_MUSB_HOST_CORE_EXPERIMENT 0
#endif
#if A04_MUSB_HOST_CORE_EXPERIMENT
/* Caller serializes all calls and owns hardware, request and buffer lifetimes.
 * This is not a Windows USB bus interface and never accesses hardware. */
#define A04_PORT_CONNECTION 0x0001u
#define A04_PORT_ENABLE     0x0002u
#define A04_PORT_OVERCURRENT 0x0008u
#define A04_PORT_RESET      0x0010u
#define A04_PORT_POWER      0x0100u
#define A04_PORT_LOW_SPEED  0x0200u
#define A04_PORT_HIGH_SPEED 0x0400u
#define A04_CHANGE_CONNECTION 0x0001u
#define A04_CHANGE_ENABLE 0x0002u
#define A04_CHANGE_OVERCURRENT 0x0008u
#define A04_CHANGE_RESET 0x0010u
#define A04_CHANGES 0x001bu
#define A04_XFER_PENDING 1
#define A04_XFER_ACTIVE 2
#define A04_XFER_ABORTING 3
#define A04_XFER_DONE 4
#define A04_RESULT_OK 0
#define A04_RESULT_CANCELLED 1
#define A04_RESULT_DISCONNECTED 2
#define A04_RESULT_FAILED 3
#define A04_QUEUE_CAPACITY 16
struct a04_root_port { unsigned status, change; };
struct a04_transfer { void *token; unsigned state, result, length, actual; };
struct a04_host_queue { struct a04_transfer *items[A04_QUEUE_CAPACITY]; unsigned count; int online; };
void a04_port_init(struct a04_root_port *p);
/* Power true is permitted only after the external owner verified VBUS. */
int a04_port_power(struct a04_root_port *p,int on,int verified);
int a04_port_connect(struct a04_root_port *p,int connected);
int a04_port_reset_begin(struct a04_root_port *p);
/* Called only after hardware reset was asserted for the required duration. */
int a04_port_reset_done(struct a04_root_port *p,unsigned speed);
int a04_port_overcurrent(struct a04_root_port *p,int active);
int a04_port_clear_change(struct a04_root_port *p,unsigned mask);
void a04_queue_init(struct a04_host_queue *q);
int a04_queue_online(struct a04_host_queue *q);
int a04_queue_submit(struct a04_host_queue *q,struct a04_transfer *t,void *token,unsigned length);
struct a04_transfer *a04_queue_start(struct a04_host_queue *q);
/* Active cancellation requires caller to quiesce endpoint/DMA before finish. */
int a04_queue_cancel(struct a04_host_queue *q,struct a04_transfer *t);
int a04_queue_finish(struct a04_host_queue *q,struct a04_transfer *t,unsigned result,unsigned actual,int quiesced);
void a04_queue_disconnect(struct a04_host_queue *q);
#endif
#endif
