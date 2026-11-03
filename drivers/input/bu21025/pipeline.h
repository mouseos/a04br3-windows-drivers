/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_TOUCH_PIPELINE_H
#define A04_TOUCH_PIPELINE_H
#include "contact.h"
#include "coordinates.h"
#include "touch_hid.h"

struct a04_touch_pipeline {
    struct a04_touch_calibration calibration;
    struct a04_touch_pressure_policy pressure;
    struct a04_touch_report_state report;
    uint8_t faulted;
};

/* Call only on a fresh state or after the owner has recovered hardware and
 * delivered any pending release. Configuration comes from the platform. All
 * operations require external serialization. */
int a04_touch_pipeline_init(struct a04_touch_pipeline *pipeline,
                            const struct a04_touch_calibration *calibration,
                            const struct a04_touch_pressure_policy *pressure);
/* 1: report, 0: no report, -1: invalid/faulted. Noisy samples preserve state. */
int a04_touch_pipeline_sample(struct a04_touch_pipeline *pipeline,
                              const struct a04_bu21025_sample *sample,
                              uint16_t scan_time, uint8_t out[A04_TOUCH_REPORT_SIZE]);
/* Latch a transport fault and, if needed, emit one release using last position.
 * The caller is responsible for retaining/delivering that release report. */
int a04_touch_pipeline_fault(struct a04_touch_pipeline *pipeline, uint16_t scan_time,
                             uint8_t out[A04_TOUCH_REPORT_SIZE]);
#endif
