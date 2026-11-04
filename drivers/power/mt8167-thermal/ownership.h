/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_THERMAL_OWNERSHIP_H
#define A04_THERMAL_OWNERSHIP_H
/* All functions are serialized by the platform. PnP resource allocation does
 * not prove EL2/EL3 or firmware relinquished AUXADC/thermal. */
struct a04_thermal_owner { unsigned generation; int resources, touch_lease, firmware_handoff, active; };
static inline int a04_owner_activate(struct a04_thermal_owner *s)
{
 if(!s || s->active || !s->resources || !s->touch_lease || !s->firmware_handoff) return -1;
 s->active=1;return 0;
}
static inline void a04_owner_revoke(struct a04_thermal_owner *s)
{ s->active=0;s->resources=0;s->touch_lease=0;s->firmware_handoff=0;s->generation++; }
#endif
