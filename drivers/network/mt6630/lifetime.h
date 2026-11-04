/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_LIFETIME_H
#define A04_MT6630_LIFETIME_H
#include "direct.h"
#include "transport.h"
/* All entry points including callbacks are serialized by the Windows owner at
 * PASSIVE_LEVEL, and callbacks may not reenter. The external broker retains
 * ownership across failure until an explicit recovery decision. No callback
 * here is a substitute for ISR synchronization, DMA teardown or PnP power. */
enum a04_wifi_life_state { A04_WIFI_LIFE_OFF, A04_WIFI_LIFE_IO,
 A04_WIFI_LIFE_STOPPING, A04_WIFI_LIFE_FAILED, A04_WIFI_LIFE_STOP_FAILED, A04_WIFI_LIFE_STOPPED };
struct a04_wifi_life_ops {
 void *context;
 int (*mask_and_drain_irq)(void *); /* disable new ISR/DPC and drain both */
 int (*drain_io)(void *);           /* no queued or in-flight requests */
 int (*unmap_host)(void *);         /* no subsequent MMIO allowed */
 int (*release_lease)(void *);      /* only after quiescence/recovery proof */
};
struct a04_wifi_life {
 enum a04_wifi_life_state state;
 struct a04_wifi_owner *owner;
 unsigned generation;
 struct a04_sdio_direct direct;
 struct a04_sdio_transport transport;
 struct a04_wifi_life_ops ops;
};
/* Control path only: valid CIS and active exclusive ownership do not prove
 * WLAN firmware, calibration, Native 802.11 or NDIS are ready. */
int a04_wifi_life_begin(struct a04_wifi_life *,struct a04_wifi_owner *,
 const uint8_t *,size_t,const struct a04_sdio_direct *,const struct a04_sdio_bus *,
 const struct a04_wifi_life_ops *);
int a04_wifi_life_cmd52(struct a04_wifi_life *,unsigned,unsigned,unsigned,unsigned,uint8_t *);
int a04_wifi_life_cmd53(struct a04_wifi_life *,unsigned,unsigned,unsigned,unsigned,uint8_t *,size_t);
/* Stop closes admission first; callback failure leaves FAILED and retains the
 * remaining resources. STOP_FAILED requires external recovery; neither it nor
 * STOPPED can be stopped again because callbacks may have partially acted. */
int a04_wifi_life_stop(struct a04_wifi_life *);
#endif
