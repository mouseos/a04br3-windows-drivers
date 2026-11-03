/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BU21025_COORDINATES_H
#define A04_BU21025_COORDINATES_H
#include <stdint.h>

/* Bounds are per-device inputs; x/y refer to the ADC axes before swapping.
 * invert_x/invert_y apply after the optional swap, in display coordinates.
 * No calibration source or default is implied by this structure. */
struct a04_touch_calibration {
    uint16_t x_min, x_max, y_min, y_max;
    uint8_t swap_axes, invert_x, invert_y;
};
struct a04_touch_position { uint16_t x, y; };

/* Board orientation in the landscape display coordinate system used by the
 * existing A04BR3 XNU port: display X decreases with ADC Y and display Y
 * decreases with ADC X. Bounds remain the caller's independent calibration. */
void a04_touch_set_board_orientation(struct a04_touch_calibration *cal);

/* Produces the full inclusive 0..65535 range. Out-of-bounds readings clamp to
 * the nearest calibrated edge. Reject invalid ADC values/configuration and
 * leave output unchanged. Caller owns calibration lifetime and synchronization. */
int a04_touch_map(const struct a04_touch_calibration *cal, uint16_t raw_x,
                  uint16_t raw_y, struct a04_touch_position *out);
#endif
