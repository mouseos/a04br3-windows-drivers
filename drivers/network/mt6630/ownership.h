/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WIFI_OWNERSHIP_H
#define A04_WIFI_OWNERSHIP_H
/* State only. Caller must serialize every transition and provide evidence;
 * flags are not a substitute for leases, Windows lifetime or firmware release. */
struct a04_wifi_owner { unsigned generation; int host, gpio_clock, combo, firmware, irq, active, poisoned; };
static inline int a04_wifi_admit(struct a04_wifi_owner *s)
{ if(!s||s->poisoned||s->active||!s->host||!s->gpio_clock||!s->combo||!s->firmware||!s->irq)return -1;s->active=1;return 0; }
static inline void a04_wifi_revoke(struct a04_wifi_owner *s)
{ if(!s)return;s->active=s->host=s->gpio_clock=s->combo=s->firmware=s->irq=0;s->generation++; }
static inline void a04_wifi_fault(struct a04_wifi_owner *s)
{ if(!s)return;a04_wifi_revoke(s);s->poisoned=1; }
#endif
