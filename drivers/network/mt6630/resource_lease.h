/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_RESOURCE_LEASE_H
#define A04_MT6630_RESOURCE_LEASE_H
#include <stdint.h>
#include "ownership.h"
/* Caller serializes every entry, including PnP, at PASSIVE_LEVEL. Providers
 * return zero only after acquiring the named real resource; a software bit is
 * not evidence of ownership. A failed operation may have partially acted, so
 * this object retains all resources and requires external recovery. */
enum a04_lease_resource { A04_LEASE_COMBO, A04_LEASE_FIRMWARE, A04_LEASE_RAIL, A04_LEASE_RTC32K,
 A04_LEASE_GPIO_CLOCK, A04_LEASE_HOST, A04_LEASE_IRQ, A04_LEASE_COUNT };
enum a04_lease_state { A04_LEASE_OFF, A04_LEASE_ACQUIRING, A04_LEASE_ACTIVE,
 A04_LEASE_STOPPING, A04_LEASE_STOPPED, A04_LEASE_FAILED };
struct a04_lease_ops {
 void *context;
 int (*acquire)(void *, enum a04_lease_resource);
 int (*release)(void *, enum a04_lease_resource);
 int (*drain_irq)(void *);
 int (*drain_io)(void *);
};
struct a04_resource_lease {
 enum a04_lease_state state;
 uint32_t generation, held;
 struct a04_wifi_owner *owner;
 struct a04_lease_ops ops;
};
int a04_resource_lease_acquire(struct a04_resource_lease *, const struct a04_lease_ops *, uint32_t *);
int a04_resource_lease_live(const struct a04_resource_lease *, uint32_t);
int a04_resource_lease_bind_owner(struct a04_resource_lease *, struct a04_wifi_owner *);
int a04_resource_lease_same_owner(void *, const struct a04_wifi_owner *, unsigned);
int a04_resource_lease_stop(struct a04_resource_lease *, uint32_t);
#endif
