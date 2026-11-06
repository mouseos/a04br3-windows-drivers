/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "host_control.h"
#if A04_MUSB_HOST_CORE_EXPERIMENT
#define INDEX 0x0eu
#define TYPE0 0x1au
#define NAKLIMIT0 0x1bu
#define TXFUNCADDR0 0x480u
#define TXHUBADDR0 0x482u
#define USB_CONNECT 0x10u
#define USB_DISCONNECT 0x20u
#define USB_VBUSERROR 0x80u
int a04_control_init(struct a04_control_host *h,const MT8167_USB20_IO *io)
{
 if(!h || !io || !io->read8 || !io->read16 || !io->write8 || !io->write16)return -1;
 h->io=*io;h->active=0;a04_port_init(&h->port);a04_queue_init(&h->queue);return 0;
}
int a04_control_submit(struct a04_control_host *h,struct a04_control_request *r)
{
 unsigned length;
 if(!h || !r || r->address>127 || (r->speed!=0x40 && r->speed!=0x80 && r->speed!=0xc0))return -1;
 length=(unsigned)r->setup[6]|((unsigned)r->setup[7]<<8);
 if((length && !r->buffer) || (r->max_packet!=8 && r->max_packet!=16 && r->max_packet!=32 && r->max_packet!=64))return -1;
 if(r->speed==0xc0 && r->max_packet!=8)return -1;
 if(r->speed==0x40 && r->max_packet!=64)return -1;
 return a04_queue_submit(&h->queue,&r->transfer,r,length);
}
int a04_control_start(struct a04_control_host *h)
{
 struct a04_transfer *t;struct a04_control_request *r;
 if(!h || h->active || (h->port.status&(A04_PORT_POWER|A04_PORT_CONNECTION|A04_PORT_ENABLE|A04_PORT_OVERCURRENT))!=(A04_PORT_POWER|A04_PORT_CONNECTION|A04_PORT_ENABLE))return -1;
 if(!h->queue.online || !h->queue.count)return -1;
 r=(struct a04_control_request *)h->queue.items[0];
 if(((h->port.status&A04_PORT_LOW_SPEED)!=0)!=(r->speed==0xc0) ||
    ((h->port.status&A04_PORT_HIGH_SPEED)!=0)!=(r->speed==0x40))return -1;
 t=a04_queue_start(&h->queue);if(!t)return -1;r=(struct a04_control_request *)t;h->active=r;
 h->io.write8(h->io.context,INDEX,0);
 h->io.write16(h->io.context,TXFUNCADDR0,(unsigned short)r->address);
 h->io.write16(h->io.context,TXHUBADDR0,0);
 h->io.write8(h->io.context,TYPE0,(unsigned char)r->speed);
 h->io.write8(h->io.context,NAKLIMIT0,0);
 if(Mt8167Ep0Start(&h->ep0,&h->io,r->setup,r->buffer,t->length,r->max_packet)!=EP0_SEND_SETUP) {
  t->state=A04_XFER_ABORTING;t->result=A04_RESULT_FAILED;return -1;
 }
 return 0;
}
int a04_control_events(struct a04_control_host *h,MT8167_USB20_EVENTS e,struct a04_control_request **done)
{
 EP0_ACTION action;struct a04_control_request *r;
 if(!h || !done)return -1;
 *done=0;
 /* Disconnect/VBUS error takes precedence over any stale EP0 completion. */
 if(e.usb&(USB_DISCONNECT|USB_VBUSERROR)) {
  if(e.usb&USB_VBUSERROR)(void)a04_port_power(&h->port,0,0);
  else (void)a04_port_connect(&h->port,0);
  a04_queue_disconnect(&h->queue);return 0;
 }
 if(e.usb&USB_CONNECT) {if(a04_port_connect(&h->port,1))return -1;return 0;}
 r=h->active;if(!(e.tx&1u) || !r || r->transfer.state!=A04_XFER_ACTIVE)return 0;
 action=Mt8167Ep0Interrupt(&h->ep0);
 if(action==EP0_ABORT) {r->transfer.state=A04_XFER_ABORTING;r->transfer.result=A04_RESULT_FAILED;return 0;}
 if(action!=EP0_COMPLETE)return 0;
 if(a04_queue_finish(&h->queue,&r->transfer,A04_RESULT_OK,h->ep0.transfer.actual,1))return -1;
 h->active=0;*done=r;return 1;
}
int a04_control_cancel(struct a04_control_host *h,struct a04_control_request *r)
{ if(!h || !r)return -1;return a04_queue_cancel(&h->queue,&r->transfer); }
int a04_control_quiesced(struct a04_control_host *h,struct a04_control_request **done)
{
 struct a04_control_request *r;if(!h || !done)return -1;*done=0;r=h->active;
 if(!r || r->transfer.state!=A04_XFER_ABORTING)return -1;
 if(a04_queue_finish(&h->queue,&r->transfer,r->transfer.result,h->ep0.transfer.actual,1))return -1;
 h->active=0;*done=r;return 0;
}
#endif
