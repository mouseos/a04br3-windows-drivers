/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT6630_POWER_BRIDGE_H
#define A04_MT6630_POWER_BRIDGE_H
#include "ownership.h"
#include "power_plan.h"
/* Serialized PASSIVE_LEVEL caller; the broker owns lifetime and guarantees
 * rail/clock/reset are exclusive, stable and safe for the entire sequence.
 * now and both delays share a monotonic caller-defined unit. This bridge
 * neither selects voltages nor enables/disables a rail or a clock. */
struct a04_wifi_power_bridge {
 struct a04_wifi_owner *owner;
 struct a04_wifi_power plan;
 void *context;
 int (*qualified)(void *,unsigned generation);
 int (*rail_clock_stable)(void *);
 int (*release_reset)(void *);
 unsigned generation;
 uint64_t rail_delay,reset_delay;
};
int a04_wifi_power_start(struct a04_wifi_power_bridge *,uint64_t now);
/* 0 ready, 1 pending, negative failure. No waiting inside callback. */
int a04_wifi_power_step(struct a04_wifi_power_bridge *,uint64_t now);
#endif
