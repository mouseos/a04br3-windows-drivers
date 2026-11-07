/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_ENUMERATE_H
#define A04_MT6630_ENUMERATE_H
#include "ownership.h"
#include <stdint.h>
struct a04_sdio_enumeration_io {
 void *context;
 int (*command)(void *,unsigned,uint32_t,unsigned,uint32_t *);
 void (*delay_us)(void *,unsigned);
};
struct a04_sdio_card_info {uint32_t ocr,rca;uint16_t vendor,device;uint8_t revision,functions;uint16_t cis_length;uint8_t cis[1024];};
/* PASSIVE_LEVEL and serialized, with exclusive owner and configured clock,
 * pins, rail and masked/drained IRQ. requested_ocr must be independently
 * qualified against the actual board rail; it never programs a voltage.
 * No Windows connection or hardware initialization is provided here. */
int a04_mt6630_enumerate(struct a04_wifi_owner *,const struct a04_sdio_enumeration_io *,
 uint32_t requested_ocr,struct a04_sdio_card_info *);
#endif
