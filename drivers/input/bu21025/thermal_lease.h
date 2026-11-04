/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_TOUCH_THERMAL_LEASE_H
#define A04_TOUCH_THERMAL_LEASE_H
#include <stdint.h>
/* Caller holds the touch provider lock. A lease never survives D0 exit. */
struct a04_thermal_lease { uintptr_t owner; int online, clocks_ready; };
static inline int a04_lease_claim(struct a04_thermal_lease *s,uintptr_t owner)
{ if(!s || !s->online || !owner || s->owner) return -1; s->owner=owner;s->clocks_ready=0;return 0; }
static inline int a04_lease_valid(const struct a04_thermal_lease *s,uintptr_t owner)
{ return s && s->online && owner && s->owner==owner; }
static inline void a04_lease_release(struct a04_thermal_lease *s,uintptr_t owner)
{ if(s && s->owner==owner) { s->owner=0;s->clocks_ready=0; } }
static inline void a04_lease_offline(struct a04_thermal_lease *s)
{ s->online=0;s->owner=0;s->clocks_ready=0; }
#endif
