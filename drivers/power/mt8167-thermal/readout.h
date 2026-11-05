/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_THERMAL_READOUT_H
#define A04_THERMAL_READOUT_H
#include "thermal.h"
/* Versioned, fixed-width ABI. Temperatures are signed millidegrees Celsius.
 * The caller serializes all access with the platform's lock. */
struct a04_thermal_readout_v1 {
 uint32_t version, sequence;
 int32_t sensor_mc[3], hottest_mc;
 uint32_t age_ms;
};
struct a04_thermal_readout_state {
 struct a04_thermal_sample sample;
 uint32_t sequence;
 int valid;
};
void a04_readout_invalidate(struct a04_thermal_readout_state *);
int a04_readout_publish(struct a04_thermal_readout_state *,const struct a04_thermal_sample *);
int a04_readout_get(const struct a04_thermal_readout_state *,uint64_t,uint32_t,struct a04_thermal_readout_v1 *);
#endif
