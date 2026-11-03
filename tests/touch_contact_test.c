/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "contact.h"
int main(void)
{
    struct a04_touch_pressure_policy p = {500, 200, 5, 900000};
    struct a04_bu21025_sample s = {0};
    unsigned x, y, z;
    assert(a04_touch_classify(&p, &s) == A04_TOUCH_RELEASE);
    s.z = 4095;
    assert(a04_touch_classify(&p, &s) == A04_TOUCH_RELEASE);
    s.z = 4096;
    assert(a04_touch_classify(&p, &s) == A04_TOUCH_INVALID);
    assert(a04_touch_classify(NULL, &s) == A04_TOUCH_INVALID);
    for (x = 0; x <= 4095; x += 103)
        for (y = 0; y <= 4095; y += 509)
            for (z = 6; z < 4095; z += 97) {
                int64_t expected = (int64_t)((500u*x)/4096u) * ((4096000u/z)-1000u)
                                   - (int64_t)(1000u-(y*1000u)/4096u)*200u;
                s.x=(uint16_t)x; s.y=(uint16_t)y; s.z=(uint16_t)z;
                assert(a04_touch_classify(&p, &s) ==
                       (expected > 900000 ? A04_TOUCH_NOISY : A04_TOUCH_DOWN));
            }
    p.x_plate_ohms = 0;
    assert(a04_touch_classify(&p, &s) == A04_TOUCH_INVALID);
    puts("touch contact classification tests passed");
    return 0;
}
