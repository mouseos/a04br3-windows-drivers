/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SD_POWER_OWNER_H
#define A04_SD_POWER_OWNER_H
#include "../../input/a04br3-buttons/pmic_read.h"
/* Embedded in the existing shared parent. All methods run under its SAME
 * blocking PWRAP/TOP lock; never instantiate another WACS2 mapping/owner. */
struct a04_sd_power {
 struct a04_pmic_read *bus;void *context;
 uint32_t (*top_read)(void *,unsigned);void (*top_write)(void *,unsigned,uint32_t);
 uint32_t (*pll_read)(void *,unsigned);uint64_t (*now_100ns)(void *);
 uint16_t saved[4];uint32_t gate1,gate2,pll_setting,hclk;unsigned held,active,poisoned,generation;
};
int a04_sd_power_acquire(struct a04_sd_power *);
int a04_sd_power_enable(struct a04_sd_power *);
int a04_sd_power_health(struct a04_sd_power *);
/* Only after consumer drained IRPs and reset the private PIO host. Failure
 * keeps held=1; caller must retain the parent's S0 power reference. */
int a04_sd_power_release(struct a04_sd_power *);
#endif
