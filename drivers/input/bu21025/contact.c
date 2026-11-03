/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Pressure equation derived from mouseos/UniXNU-a04br3 A04BR3Input.cpp.
 */
#include "contact.h"

/* Bounded unsigned division without a kernel compiler-runtime dependency. */
static uint32_t divide_u32(uint32_t numerator, uint32_t denominator)
{
    uint32_t quotient = 0, remainder = 0;
    unsigned i;
    for (i = 0; i < 32; ++i) {
        remainder = (remainder << 1) | (numerator >> 31);
        numerator <<= 1; quotient <<= 1;
        if (remainder >= denominator) { remainder -= denominator; quotient |= 1; }
    }
    return quotient;
}

enum a04_touch_contact a04_touch_classify(const struct a04_touch_pressure_policy *policy,
                                          const struct a04_bu21025_sample *sample)
{
    uint32_t x_component, ratio, y_component;
    int64_t resistance;
    if (!policy || !sample || !policy->x_plate_ohms || !policy->y_plate_ohms ||
        policy->x_plate_ohms > 10000 || policy->y_plate_ohms > 10000 ||
        policy->release_z_max >= 4095 || !policy->max_resistance_milliohms ||
        sample->x > 4095 || sample->y > 4095 || sample->z > 4095)
        return A04_TOUCH_INVALID;
    if (sample->z <= policy->release_z_max || sample->z == 4095)
        return A04_TOUCH_RELEASE;
    x_component = ((uint32_t)policy->x_plate_ohms * sample->x) >> 12;
    ratio = divide_u32(4096000u, sample->z) - 1000u;
    y_component = 1000u - (((uint32_t)sample->y * 1000u) >> 12);
    resistance = (int64_t)x_component * ratio - (int64_t)y_component * policy->y_plate_ohms;
    return resistance > (int64_t)policy->max_resistance_milliohms ? A04_TOUCH_NOISY : A04_TOUCH_DOWN;
}
