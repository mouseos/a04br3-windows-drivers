/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_TOUCH_DEVICE_H
#define A04_TOUCH_DEVICE_H
#include "pipeline.h"

struct a04_touch_device {
    struct a04_bu21025_io io;
    struct a04_touch_pipeline pipeline;
    uint8_t ready;
};
enum a04_touch_poll_result { A04_POLL_FAULT = -1, A04_POLL_IDLE = 0,
                             A04_POLL_REPORT = 1, A04_POLL_FAULT_REPORT = 2 };

/* The owner must power and configure the bus, pinmux and controller first,
 * supply per-device configuration and serialize access. Reinitialization is
 * allowed only after hardware recovery and delivery of a pending release. */
int a04_touch_device_init(struct a04_touch_device *device,
                          const struct a04_bu21025_io *io,
                          const struct a04_touch_calibration *calibration,
                          const struct a04_touch_pressure_policy *pressure);
/* pen_down is obtained by the caller from the active-low pen GPIO. It gates
 * only initial sampling; an existing contact is released from ADC Z. A transport
 * fault permanently stops polling until recovery. FAULT_REPORT means the
 * release must be delivered before recovering; FAULT means no report. */
enum a04_touch_poll_result a04_touch_device_poll(struct a04_touch_device *device,
                                                 int pen_down, uint16_t scan_time,
                                                 uint8_t out[A04_TOUCH_REPORT_SIZE]);
#endif
