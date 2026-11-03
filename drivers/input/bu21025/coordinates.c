/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "coordinates.h"

/* Avoid depending on an ARM compiler division runtime in kernel builds.
 * The caller bounds denominator to 1..4095 and numerator below 2^28. */
static uint32_t divide_u32(uint32_t numerator, uint32_t denominator)
{
    uint32_t quotient = 0, remainder = 0;
    unsigned i;
    for (i = 0; i < 32; ++i) {
        remainder = (remainder << 1) | (numerator >> 31);
        numerator <<= 1;
        quotient <<= 1;
        if (remainder >= denominator) { remainder -= denominator; quotient |= 1; }
    }
    return quotient;
}

static uint16_t normalize(uint16_t value, uint16_t min, uint16_t max)
{
    uint32_t scaled;
    if (value <= min) return 0;
    if (value >= max) return 65535;
    scaled = (uint32_t)(value - min) * 65535u;
    return (uint16_t)divide_u32(scaled, (uint32_t)(max - min));
}

int a04_touch_map(const struct a04_touch_calibration *cal, uint16_t raw_x,
                  uint16_t raw_y, struct a04_touch_position *out)
{
    uint16_t x, y, tmp;
    if (!cal || !out || raw_x > 4095 || raw_y > 4095 ||
        cal->x_min >= cal->x_max || cal->y_min >= cal->y_max ||
        cal->x_max > 4095 || cal->y_max > 4095 ||
        cal->swap_axes > 1 || cal->invert_x > 1 || cal->invert_y > 1)
        return -1;
    x = normalize(raw_x, cal->x_min, cal->x_max);
    y = normalize(raw_y, cal->y_min, cal->y_max);
    if (cal->swap_axes) { tmp = x; x = y; y = tmp; }
    if (cal->invert_x) x = (uint16_t)(65535u - x);
    if (cal->invert_y) y = (uint16_t)(65535u - y);
    out->x = x; out->y = y;
    return 0;
}
