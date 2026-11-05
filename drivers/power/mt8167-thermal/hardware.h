/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_MT8167_THERMAL_HARDWARE_H
#define A04_MT8167_THERMAL_HARDWARE_H
#include <stdint.h>
#include "thermal.h"
/* Candidate sequencing only; caller holds exclusive thermal/AUXADC/clock
 * ownership for its lifetime. No MMIO access occurs until qualified is true. */
struct a04_thermal_hw {
 void *ctx;
 int (*qualified)(void *);
 int (*clocks_on)(void *);
 int (*initialized)(void *);
 int (*read32)(void *,uint32_t,uint32_t *);
 int (*write32)(void *,uint32_t,uint32_t);
 int (*delay_us)(void *,unsigned);
};
int a04_thermal_hw_init(const struct a04_thermal_hw *);
/* Returns success only after pause, disable and AHB-idle observations. */
int a04_thermal_hw_quiesce(const struct a04_thermal_hw *);
int a04_thermal_hw_protect(const struct a04_thermal_hw *, const struct a04_thermal_cal *, int32_t);
#endif
