/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_THERMAL_ONESHOT_H
#define A04_THERMAL_ONESHOT_H
#include "completion.h"
/* Diagnostic only. The caller owns resources from before entry until return,
 * and keeps them quarantined if shutdown fails. Report success is delivery to
 * the callback, not proof of UART transmission or calibrated accuracy. */
struct a04_oneshot_io {
 const struct a04_thermal_hw *hw;
 void *context;
 uint64_t (*now_ms)(void *);
 int (*wait_ms)(void *,unsigned);
 int (*report)(void *,const struct a04_thermal_sample *);
};
enum a04_oneshot_result { A04_ONESHOT_OK, A04_ONESHOT_INVALID,
 A04_ONESHOT_INIT_FAILED, A04_ONESHOT_TIMEOUT, A04_ONESHOT_SAMPLE_FAILED,
 A04_ONESHOT_SHUTDOWN_FAILED, A04_ONESHOT_REPORT_FAILED };
enum a04_oneshot_result a04_thermal_oneshot(const struct a04_oneshot_io *,const struct a04_thermal_cal *);
#endif
