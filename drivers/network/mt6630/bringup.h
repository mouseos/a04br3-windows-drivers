/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WIFI_BRINGUP_H
#define A04_WIFI_BRINGUP_H
#include "board_sequence.h"
#include "control_start.h"
/* Serialized at PASSIVE_LEVEL with IRQs drained. same_lease must verify that
 * board I/O and MSDC2 refer to the same still-live broker lease and generation.
 * On any failure after board writes, retain resources for external recovery.
 * Successful SDIO enumeration does not establish WLAN or NDIS readiness. */
int a04_wifi_bringup(struct a04_wifi_board *,const struct a04_wifi_board_io *,
 struct a04_wifi_life *,struct a04_msdc2_pio *,uint32_t,
 const struct a04_wifi_life_ops *,struct a04_sdio_card_info *,
 int (*same_lease)(void *,const struct a04_wifi_owner *,unsigned),void *);
#endif
