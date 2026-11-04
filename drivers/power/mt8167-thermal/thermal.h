/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_MT8167_THERMAL_H
#define A04_MT8167_THERMAL_H
#include <stdint.h>
#define A04_TEMP_SENSORS 3
/* Indices from the bootloader's devinfo record; no device-specific values. */
struct a04_thermal_cal { int32_t gain, offset, room[3], degc, slope; };
struct a04_thermal_sample { int32_t mc[3], hottest_mc; uint64_t time_ms; };
typedef int (*a04_devinfo_read)(void *, unsigned, uint32_t *);
int a04_thermal_calibrate(struct a04_thermal_cal *, a04_devinfo_read, void *);
int a04_thermal_threshold(const struct a04_thermal_cal *, int32_t, uint16_t *);
int a04_thermal_convert(const struct a04_thermal_cal *, unsigned, uint16_t, int32_t *);
int a04_thermal_sample(const struct a04_thermal_cal *, const uint32_t[3], uint64_t,
                       int, struct a04_thermal_sample *);
enum a04_thermal_action { A04_TEMP_HALT, A04_TEMP_LIMIT, A04_TEMP_NORMAL };
struct a04_thermal_policy { int32_t limit_mc, release_mc, halt_mc; uint64_t max_age_ms; int limited; };
enum a04_thermal_action a04_thermal_decide(struct a04_thermal_policy *,
                                            const struct a04_thermal_sample *,
                                            uint64_t, int);
#endif
