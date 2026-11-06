/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_TRANSPORT_H
#define A04_MT6630_TRANSPORT_H
#include <stddef.h>
#include <stdint.h>
/* Callbacks must be serialized, bound to a single power/host owner and return
 * only after completion or cancellation. Timeout leaves the owner poisoned.
 * This module has no MMIO or OS integration. */
struct a04_sdio_bus {
 void *context;
 int (*request)(void *,uint32_t,unsigned,uint8_t *,size_t);
};
struct a04_sdio_transport { struct a04_sdio_bus bus;int ready,poisoned; };
int a04_sdio_transfer(struct a04_sdio_transport *,unsigned,unsigned,unsigned,unsigned,uint8_t *,size_t);
#endif
