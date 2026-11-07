/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_CALIBRATION_H
#define A04_WINDOWS_CALIBRATION_H
#include "windows_firmware.h"
/* Serialized PASSIVE_LEVEL stage after WLAN boot. Requires live lease.
 * Reads this device at runtime; success is submission, never RF readiness. */
int a04_windows_calibration_load(struct a04_wmt_bridge *);
int a04_windows_calibration_load_traced(struct a04_wmt_bridge *,a04_session_trace_fn,void *);
#endif
