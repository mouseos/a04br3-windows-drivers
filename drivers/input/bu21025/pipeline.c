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
    pipeline->history_count = 0;
    pipeline->faulted = 0;
    return 0;
}

static uint16_t median3(const uint16_t v[3])
{
    uint16_t a = v[0], b = v[1], c = v[2];
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) b = c;
    return a > b ? a : b;
}

int a04_touch_pipeline_sample(struct a04_touch_pipeline *pipeline,
                              const struct a04_bu21025_sample *sample,
                              uint16_t scan_time, uint8_t out[A04_TOUCH_REPORT_SIZE])
{
    enum a04_touch_contact contact;
    struct a04_touch_position position;
    uint16_t x, y;
    unsigned slot;
    if (!pipeline || !sample || !out || pipeline->faulted) return -1;
    contact = a04_touch_classify(&pipeline->pressure, sample);
    if (contact == A04_TOUCH_INVALID) return -1;
    if (contact == A04_TOUCH_NOISY) return 0;
    if (contact == A04_TOUCH_RELEASE) {
        pipeline->history_count = 0;
        return a04_touch_report(&pipeline->report, 0, 0, 0, scan_time, out);
    }
    slot = pipeline->history_count < 3 ? pipeline->history_count : 0;
    if (pipeline->history_count >= 3) {
        pipeline->history_x[0] = pipeline->history_x[1]; pipeline->history_x[1] = pipeline->history_x[2];
        pipeline->history_y[0] = pipeline->history_y[1]; pipeline->history_y[1] = pipeline->history_y[2];
        slot = 2;
    } else pipeline->history_count++;
    pipeline->history_x[slot] = (uint16_t)sample->x;
    pipeline->history_y[slot] = (uint16_t)sample->y;
    if (pipeline->history_count >= 3) {
        x = median3(pipeline->history_x); y = median3(pipeline->history_y);
    } else { x = (uint16_t)sample->x; y = (uint16_t)sample->y; }
    if (a04_touch_map(&pipeline->calibration, x, y, &position)) return -1;
    return a04_touch_report(&pipeline->report, 1, position.x, position.y, scan_time, out);
}

int a04_touch_pipeline_fault(struct a04_touch_pipeline *pipeline, uint16_t scan_time,
                             uint8_t out[A04_TOUCH_REPORT_SIZE])
{
    if (!pipeline || !out || pipeline->faulted) return -1;
    pipeline->faulted = 1;
    return a04_touch_report(&pipeline->report, 0, 0, 0, scan_time, out);
}
