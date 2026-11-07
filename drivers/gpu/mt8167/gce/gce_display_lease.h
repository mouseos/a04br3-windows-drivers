/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GCE_DISPLAY_LEASE_H
#define A04_GCE_DISPLAY_LEASE_H
#include <stdint.h>
/* Protected by the GCE provider's passive-level lease lock. A successful
 * lease protects the WDF owner object, never PnP power or hardware access. */
struct a04_gce_display_lease { uint32_t generation,active;int online,removed; };
static int a04_gce_display_acquire(struct a04_gce_display_lease *s,uint32_t *token)
{
    if(!s || !token)return 0;
    *token=0;
    if(!s->online || s->removed || s->active || s->generation==UINT32_MAX)return 0;
    s->active=++s->generation;*token=s->active;return 1;
}
static int a04_gce_display_release(struct a04_gce_display_lease *s,uint32_t token)
{
    if(!s || !token || s->active!=token)return 0;
    s->active=0;return 1;
}
static int a04_gce_display_is_live(const struct a04_gce_display_lease *s,uint32_t token)
{ return s && token && s->active==token && s->online && !s->removed; }
#endif
