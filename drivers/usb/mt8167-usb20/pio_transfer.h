/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MUSB_PIO_TRANSFER_H
#define A04_MUSB_PIO_TRANSFER_H
#include "ep0_pio.h"
/* Single serialized hardware EP1, TX/RX 64-byte banks. Direct root-port
 * devices only: no TT/split, isochronous or >64-byte packets are advertised. */
struct a04_pio_endpoint {unsigned address,endpoint,packet,speed,type,direction,period,toggle;};
struct a04_pio_transfer {struct a04_pio_endpoint ep;unsigned char *buffer;unsigned bytes,actual,pending,zlp,active;};
#define A04_PIO_PENDING 0
#define A04_PIO_DONE 1
#define A04_PIO_STALL -2
#define A04_PIO_ERROR -1
int a04_pio_configure(const MT8167_USB20_IO *,struct a04_pio_transfer *,const struct a04_pio_endpoint *,unsigned char *,unsigned);
int a04_pio_event(const MT8167_USB20_IO *,struct a04_pio_transfer *);
/* Returns failure unless both FIFOs prove stopped after flush. No completion
 * or memory release is allowed when quiescence cannot be observed. */
int a04_pio_abort(const MT8167_USB20_IO *,struct a04_pio_transfer *);
#endif
