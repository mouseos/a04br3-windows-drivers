/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Experimental Windows 8 touch usages, informed by Microsoft digitizer docs.
 */
#include "touch_hid.h"

static void append(uint8_t *out, size_t *n, const uint8_t *data, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i) out[(*n)++] = data[i];
}
static void physical_max(uint8_t *out, size_t *n, uint16_t value)
{
    out[(*n)++] = 0x47; out[(*n)++] = (uint8_t)value;
    out[(*n)++] = (uint8_t)(value >> 8); out[(*n)++] = 0; out[(*n)++] = 0;
}

int a04_touch_descriptor(uint16_t width, uint16_t height,
                         uint8_t *out, size_t capacity, size_t *length)
{
    static const uint8_t prefix[] = {
        0x05,0x0d, 0x09,0x04, 0xa1,0x01, 0x85,0x01,
        0x09,0x22, 0xa1,0x02, 0x09,0x42, 0x15,0x00, 0x25,0x01,
        0x75,0x01, 0x95,0x01, 0x81,0x02, 0x95,0x07, 0x81,0x03,
        0x75,0x08, 0x95,0x01, 0x09,0x51, 0x81,0x02,
        0x05,0x01, 0x09,0x30, 0x15,0x00, 0x27,0xff,0xff,0x00,0x00,
        0x35,0x00
    };
    static const uint8_t x_tail[] = {0x55,0x0e, 0x65,0x13, 0x75,0x10, 0x95,0x01, 0x81,0x02, 0x09,0x31};
    static const uint8_t y_tail[] = {
        0x81,0x02, 0xc0,
        0x05,0x0d, 0x09,0x56, 0x15,0x00, 0x27,0xff,0xff,0x00,0x00,
        0x35,0x00, 0x47,0xff,0xff,0x00,0x00, 0x55,0x0c, 0x66,0x01,0x10,
        0x75,0x10, 0x95,0x01, 0x81,0x02,
        0x09,0x54, 0x15,0x00, 0x25,0x01, 0x35,0x00, 0x45,0x01,
        0x55,0x00, 0x65,0x00, 0x75,0x08, 0x95,0x01, 0x81,0x02,
        0x85,0x02, 0x09,0x55, 0xb1,0x02, 0xc0
    };
    _Static_assert(sizeof(prefix) + sizeof(x_tail) + sizeof(y_tail) + 10 <=
                   A04_TOUCH_DESCRIPTOR_CAPACITY, "touch descriptor buffer too small");
    uint8_t tmp[A04_TOUCH_DESCRIPTOR_CAPACITY];
    size_t n = 0, i;
    if (!width || !height || !out || !length) return -1;
    append(tmp, &n, prefix, sizeof(prefix)); physical_max(tmp, &n, width);
    append(tmp, &n, x_tail, sizeof(x_tail)); physical_max(tmp, &n, height);
    append(tmp, &n, y_tail, sizeof(y_tail));
    if (capacity < n) return -1;
    for (i = 0; i < n; ++i) out[i] = tmp[i];
    *length = n;
    return 0;
}

int a04_touch_report(struct a04_touch_report_state *state, int down,
                     uint16_t x, uint16_t y, uint16_t scan_time,
                     uint8_t out[A04_TOUCH_REPORT_SIZE])
{
    if (!state || !out || (down != 0 && down != 1) || state->down > 1) return -1;
    if (!down && !state->down) return 0;
    if (down) { state->last_x = x; state->last_y = y; }
    x = state->last_x; y = state->last_y;
    out[0] = 1; out[1] = (uint8_t)down; out[2] = 0;
    out[3] = (uint8_t)x; out[4] = (uint8_t)(x >> 8);
    out[5] = (uint8_t)y; out[6] = (uint8_t)(y >> 8);
    out[7] = (uint8_t)scan_time; out[8] = (uint8_t)(scan_time >> 8);
    out[9] = 1; /* one reported contact, including its final release */
    state->down = (uint8_t)down;
    return 1;
}

int a04_touch_max_contacts(uint8_t out[A04_TOUCH_FEATURE_SIZE])
{
    if (!out) return -1;
    out[0] = 2; out[1] = 1;
    return 0;
}
