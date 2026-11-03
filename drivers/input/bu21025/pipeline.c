/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "pipeline.h"

int a04_touch_pipeline_init(struct a04_touch_pipeline *pipeline,
                            const struct a04_touch_calibration *calibration,
                            const struct a04_touch_pressure_policy *pressure)
{
    struct a04_touch_position position;
    const struct a04_bu21025_sample zero = {0,0,0};
    if (!pipeline || !calibration || !pressure ||
        a04_touch_map(calibration, 0, 0, &position) != 0 ||
        a04_touch_classify(pressure, &zero) == A04_TOUCH_INVALID) return -1;
    pipeline->calibration = *calibration; pipeline->pressure = *pressure;
    pipeline->report = (struct a04_touch_report_state){0,0,0};
    pipeline->faulted = 0;
    return 0;
}

int a04_touch_pipeline_sample(struct a04_touch_pipeline *pipeline,
                              const struct a04_bu21025_sample *sample,
                              uint16_t scan_time, uint8_t out[A04_TOUCH_REPORT_SIZE])
{
    enum a04_touch_contact contact;
    struct a04_touch_position position;
    if (!pipeline || !sample || !out || pipeline->faulted) return -1;
    contact = a04_touch_classify(&pipeline->pressure, sample);
    if (contact == A04_TOUCH_INVALID) return -1;
    if (contact == A04_TOUCH_NOISY) return 0;
    if (contact == A04_TOUCH_RELEASE)
        return a04_touch_report(&pipeline->report, 0, 0, 0, scan_time, out);
    if (a04_touch_map(&pipeline->calibration, sample->x, sample->y, &position)) return -1;
    return a04_touch_report(&pipeline->report, 1, position.x, position.y, scan_time, out);
}

int a04_touch_pipeline_fault(struct a04_touch_pipeline *pipeline, uint16_t scan_time,
                             uint8_t out[A04_TOUCH_REPORT_SIZE])
{
    if (!pipeline || !out || pipeline->faulted) return -1;
    pipeline->faulted = 1;
    return a04_touch_report(&pipeline->report, 0, 0, 0, scan_time, out);
}
