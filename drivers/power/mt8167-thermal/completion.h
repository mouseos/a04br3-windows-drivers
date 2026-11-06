/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_THERMAL_COMPLETION_H
#define A04_THERMAL_COMPLETION_H
#include "freshness.h"
#include "hardware.h"
struct a04_completion { struct a04_freshness freshness; uint64_t start_ms, timeout_ms; int initialized; };
/* Call only after the synthetic ADC self-test has been observed on all three
 * channels and normal mode has been configured. Evidence, not proof of ADC
 * accuracy or exclusive ownership. Failure is sticky until reinitialization. */
int a04_completion_start(struct a04_completion *,uint64_t,uint64_t);
/* 1=all channels changed from synthetic values and look active; 0=waiting;
 * -1=fault. Writes out only on success. */
int a04_completion_poll(struct a04_completion *,const struct a04_thermal_hw *,uint64_t,uint64_t,uint32_t out[3]);
#endif
