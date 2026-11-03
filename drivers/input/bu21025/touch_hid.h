/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_TOUCH_HID_H
#define A04_TOUCH_HID_H
#include <stddef.h>
#include <stdint.h>

/* Experimental descriptor and one-contact report encoder; no PnP/HID driver.
 * Physical sizes are supplied in 1/100 inch and must describe the panel,
 * not be inferred from its pixel dimensions. No per-device value is built in. */
#define A04_TOUCH_DESCRIPTOR_CAPACITY 192u
#define A04_TOUCH_REPORT_SIZE 10u
#define A04_TOUCH_FEATURE_SIZE 2u
int a04_touch_descriptor(uint16_t width, uint16_t height,
                         uint8_t *out, size_t capacity, size_t *length);

struct a04_touch_report_state { uint16_t last_x, last_y; uint8_t down; };
/* Return 1 for a report, 0 for idle, -1 for invalid input. On release retain
 * the last down position and report one contact with tip clear. scan_time is
 * in 100-us units and may wrap; its epoch is chosen by the caller. */
int a04_touch_report(struct a04_touch_report_state *state, int down,
                     uint16_t x, uint16_t y, uint16_t scan_time,
                     uint8_t out[A04_TOUCH_REPORT_SIZE]);
int a04_touch_max_contacts(uint8_t out[A04_TOUCH_FEATURE_SIZE]);
#endif
