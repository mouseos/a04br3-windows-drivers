/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BU21025_CONTACT_H
#define A04_BU21025_CONTACT_H
#include <stdint.h>
#include "bu21025.h"

/* Parameters are supplied by the platform; no tablet-specific calibration
 * or resistance thresholds are embedded in the component. */
struct a04_touch_pressure_policy {
    uint16_t x_plate_ohms, y_plate_ohms, release_z_max;
    uint32_t max_resistance_milliohms;
};
enum a04_touch_contact { A04_TOUCH_INVALID = -1, A04_TOUCH_RELEASE = 0,
                         A04_TOUCH_DOWN = 1, A04_TOUCH_NOISY = 2 };
enum a04_touch_contact a04_touch_classify(const struct a04_touch_pressure_policy *policy,
                                          const struct a04_bu21025_sample *sample);
#endif
