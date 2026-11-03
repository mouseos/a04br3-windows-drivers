/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "coordinates.h"
int main(void)
{
    struct a04_touch_calibration c = {100, 1100, 200, 2200, 0, 0, 0};
    struct a04_touch_position p = {7, 9};
    unsigned x, max;
    assert(a04_touch_map(&c, 100, 2200, &p) == 0 && p.x == 0 && p.y == 65535);
    assert(a04_touch_map(&c, 600, 1200, &p) == 0 && p.x == 32767 && p.y == 32767);
    assert(a04_touch_map(&c, 0, 4095, &p) == 0 && p.x == 0 && p.y == 65535);
    c.swap_axes = 1; c.invert_x = 1; c.invert_y = 1;
    assert(a04_touch_map(&c, 100, 2200, &p) == 0 && p.x == 0 && p.y == 65535);
    p.x = 7; p.y = 9; c.x_max = c.x_min;
    assert(a04_touch_map(&c, 100, 200, &p) != 0 && p.x == 7 && p.y == 9);
    c.x_max = 1100;
    assert(a04_touch_map(&c, 4096, 200, &p) != 0 && p.x == 7 && p.y == 9);
    assert(a04_touch_map(NULL, 0, 0, &p) != 0 && a04_touch_map(&c, 0, 0, NULL) != 0);
    /* Same axis direction as A04BR3Input.cpp; bounds are illustrative only. */
    c = (struct a04_touch_calibration){100, 1100, 200, 2200, 0, 0, 0};
    a04_touch_set_board_orientation(&c);
    assert(c.x_min == 100 && c.x_max == 1100 && c.y_min == 200 && c.y_max == 2200);
    assert(a04_touch_map(&c, 100, 200, &p) == 0 && p.x == 65535 && p.y == 65535);
    assert(a04_touch_map(&c, 1100, 200, &p) == 0 && p.x == 65535 && p.y == 0);
    assert(a04_touch_map(&c, 100, 2200, &p) == 0 && p.x == 0 && p.y == 65535);
    assert(a04_touch_map(&c, 1100, 2200, &p) == 0 && p.x == 0 && p.y == 0);
    a04_touch_set_board_orientation(NULL);
    c = (struct a04_touch_calibration){0, 4095, 0, 4095, 0, 0, 0};
    for (max = 1; max <= 4095; max += 17) {
        c.x_max = (uint16_t)max;
        for (x = 0; x <= 4095; x += 19) {
            unsigned expected = x >= max ? 65535 : (x * 65535u) / max;
            assert(a04_touch_map(&c, (uint16_t)x, 0, &p) == 0 && p.x == expected);
        }
    }
    puts("touch coordinate tests passed");
    return 0;
}
