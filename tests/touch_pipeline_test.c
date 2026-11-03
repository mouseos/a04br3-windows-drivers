/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pipeline.h"
int main(void)
{
    const struct a04_touch_calibration cal = {0,4095,0,4095,0,0,0};
    const struct a04_touch_pressure_policy pressure = {500,200,5,900000};
    struct a04_touch_pipeline pipeline;
    struct a04_bu21025_sample sample = {1000,1000,1000};
    uint8_t report[A04_TOUCH_REPORT_SIZE], down[A04_TOUCH_REPORT_SIZE];
    assert(a04_touch_pipeline_init(&pipeline, &cal, &pressure) == 0);
    assert(a04_touch_pipeline_sample(&pipeline, &sample, 10, report) == 1);
    assert(report[0] == 1 && report[1] == 1 && report[9] == 1);
    memcpy(down, report, sizeof(down));
    sample.x = 4000; sample.z = 6;
    assert(a04_touch_pipeline_sample(&pipeline, &sample, 11, report) == 0);
    sample.z = 0;
    assert(a04_touch_pipeline_sample(&pipeline, &sample, 12, report) == 1);
    assert(report[1] == 0 && memcmp(report + 3, down + 3, 4) == 0 && report[9] == 1);
    assert(a04_touch_pipeline_sample(&pipeline, &sample, 13, report) == 0);
    sample = (struct a04_bu21025_sample){1000,1000,1000};
    assert(a04_touch_pipeline_sample(&pipeline, &sample, 14, report) == 1);
    memcpy(down, report, sizeof(down));
    assert(a04_touch_pipeline_fault(&pipeline, 15, report) == 1 && report[1] == 0);
    assert(memcmp(report + 3, down + 3, 4) == 0);
    assert(a04_touch_pipeline_sample(&pipeline, &sample, 16, report) == -1);
    assert(a04_touch_pipeline_fault(&pipeline, 17, report) == -1);
    puts("touch pipeline tests passed");
    return 0;
}
