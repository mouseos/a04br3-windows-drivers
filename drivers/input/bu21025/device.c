/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "device.h"

int a04_touch_device_init(struct a04_touch_device *device,
                          const struct a04_bu21025_io *io,
                          const struct a04_touch_calibration *calibration,
                          const struct a04_touch_pressure_policy *pressure)
{
    if (!device || !io) return -1;
    device->ready = 0;
    if (a04_touch_pipeline_init(&device->pipeline, calibration, pressure)) return -1;
    device->io = *io;
    if (a04_bu21025_init(&device->io)) { device->pipeline.faulted = 1; return -1; }
    device->ready = 1;
    return 0;
}

enum a04_touch_poll_result a04_touch_device_poll(struct a04_touch_device *device,
                                                 int pen_down, uint16_t scan_time,
                                                 uint8_t out[A04_TOUCH_REPORT_SIZE])
{
    struct a04_bu21025_sample sample;
    int result;
    if (!device || !out || !device->ready || (pen_down != 0 && pen_down != 1))
        return A04_POLL_FAULT;
    /* The pen interrupt gates initial sampling only. Its deassertion does
     * not establish release during a contact: keep sampling Z until the ADC
     * reports release, as in the board Linux and XNU implementations. */
    if (!pen_down && !device->pipeline.report.down) {
        sample = (struct a04_bu21025_sample){0,0,0};
    } else if (a04_bu21025_sample(&device->io, &sample)) {
        result = a04_touch_pipeline_fault(&device->pipeline, scan_time, out);
        device->ready = 0;
        return result == 1 ? A04_POLL_FAULT_REPORT : A04_POLL_FAULT;
    }
    result = a04_touch_pipeline_sample(&device->pipeline, &sample, scan_time, out);
    if (result < 0) {
        result = a04_touch_pipeline_fault(&device->pipeline, scan_time, out);
        device->ready = 0;
        return result == 1 ? A04_POLL_FAULT_REPORT : A04_POLL_FAULT;
    }
    return result ? A04_POLL_REPORT : A04_POLL_IDLE;
}
