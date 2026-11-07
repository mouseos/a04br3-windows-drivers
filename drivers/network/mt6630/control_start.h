/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_CONTROL_START_H
#define A04_MT6630_CONTROL_START_H
#include "enumerate.h"
#include "lifetime.h"
#include "msdc2_pio.h"
/* Serialized PASSIVE_LEVEL only. The real broker must already have granted and
 * kept the lease, configured the host/pins/rail/clock and quiesced the IRQ.
 * ocr is independently board-qualified; 512-byte block mode remains disabled
 * unless the caller later negotiates that FBR setting. No NDIS/radio readiness. */
int a04_wifi_control_start(struct a04_wifi_life *,struct a04_msdc2_pio *,
 uint32_t ocr,const struct a04_wifi_life_ops *,struct a04_sdio_card_info *);
#endif
