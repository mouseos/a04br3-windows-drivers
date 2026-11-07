/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_THERMAL_STOP_H
#define A04_THERMAL_STOP_H
#include "hardware.h"
struct a04_thermal_stop_state { unsigned touched,irq_masked,irq_drained,ahb_idle,periodic_off,quarantined; };
/* drain_irq must be a real supplier interrupt/pending/rundown implementation;
 * no existing platform implementation qualifies this callback. Mask readback
 * does not prove drain. State lives with supplier, never transient child IO. */
struct a04_thermal_stop_io { const struct a04_thermal_hw *hw;void *context;int (*drain_irq)(void *); };
/* No clocks/analog/resources are returned here. Nonzero after touched means
 * retained quarantine, never OS panic or permission to free unknown DMA. */
int a04_thermal_stop(struct a04_thermal_stop_state *,const struct a04_thermal_stop_io *);
#endif
