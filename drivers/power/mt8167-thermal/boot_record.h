/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_THERMAL_BOOT_RECORD_H
#define A04_THERMAL_BOOT_RECORD_H
#include "thermal.h"
struct a04_thermal_boot_cal { uint32_t version,d60,d61,d62; };
/* Exact volatile BS|RT contract. No native pointer or host/per-device default.
 * Calendar-style structural validation is not ADC/firmware ownership. */
int a04_thermal_boot_calibration(struct a04_thermal_cal *,const void *,unsigned,unsigned);
#endif
