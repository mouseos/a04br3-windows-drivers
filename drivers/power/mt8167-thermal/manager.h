/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_MT8167_THERMAL_MANAGER_H
#define A04_MT8167_THERMAL_MANAGER_H
#include "thermal.h"
/* The platform must supply exclusive ownership, freshness, and real control.
 * A successful callback must actually apply/acknowledge the requested action. */
struct a04_thermal_ops {
    void *ctx;
    a04_devinfo_read read_devinfo;
    int (*acquire)(void *);
    void (*release)(void *);
    int (*read_fresh)(void *, uint32_t raw[3], uint64_t *sample_ms);
    int (*apply)(void *, enum a04_thermal_action);
};
struct a04_thermal_manager {
    struct a04_thermal_cal cal;
    struct a04_thermal_policy policy;
    int ready;
};
int a04_thermal_start(struct a04_thermal_manager *, const struct a04_thermal_ops *,
                      const struct a04_thermal_policy *);
int a04_thermal_poll(struct a04_thermal_manager *, const struct a04_thermal_ops *, uint64_t,
                     struct a04_thermal_sample *);
#endif
