/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_DIRECT_H
#define A04_MT6630_DIRECT_H
#include <stdint.h>
#include "ownership.h"
/* Caller serializes all calls and revocations, and keeps the lease alive until
 * request returns. Callback must complete or cancel within its own deadline.
 * No OS, clock, power, reset or MMIO access is provided here. */
struct a04_sdio_direct {
 struct a04_wifi_owner *owner;
 void *context;
 int (*request)(void *context, uint32_t argument, uint32_t *r5);
 unsigned generation;
 int poisoned;
};
int a04_sdio_direct52(struct a04_sdio_direct *, unsigned write, unsigned fn,
                      unsigned raw, unsigned address, uint8_t *data);
#endif
