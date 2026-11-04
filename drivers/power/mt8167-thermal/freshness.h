/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_THERMAL_FRESHNESS_H
#define A04_THERMAL_FRESHNESS_H
#include <stdint.h>
struct a04_freshness { uint16_t last[3]; uint64_t changed_ms[3], last_ms; unsigned seen, changed; };
/* A change provides evidence of activity, not proof of valid calibration or
 * analog accuracy. Unchanged sensors time out even at a steady temperature. */
int a04_freshness_update(struct a04_freshness *,const uint32_t[3],uint64_t,uint64_t);
#endif
