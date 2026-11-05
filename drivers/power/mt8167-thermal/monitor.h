/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_THERMAL_MONITOR_H
#define A04_THERMAL_MONITOR_H
#include "completion.h"
struct a04_monitor {
 struct a04_thermal_cal calibration;
 struct a04_thermal_policy policy;
 struct a04_completion completion;
 const struct a04_thermal_hw *hw;
 void *context;
 int (*apply)(void *, enum a04_thermal_action);
 int active;
};
/* No platform may call start without an independently established resource
 * and power/firmware ownership contract. LIMIT/HALT must be real actuators. */
int a04_monitor_start(struct a04_monitor *,const struct a04_thermal_hw *,
                      const struct a04_thermal_cal *,const struct a04_thermal_policy *,
                      void *,int (*)(void *,enum a04_thermal_action),uint64_t,uint64_t);
/* 1=valid reading, 0=waiting, -1=stop requested, -2=stop request failed. No outputs on failure. */
int a04_monitor_poll(struct a04_monitor *,uint64_t,struct a04_thermal_sample *);
#endif
