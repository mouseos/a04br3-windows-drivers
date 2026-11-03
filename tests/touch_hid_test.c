/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "touch_hid.h"
int main(void)
{
    uint8_t descriptor[A04_TOUCH_DESCRIPTOR_CAPACITY], report[A04_TOUCH_REPORT_SIZE];
    uint8_t feature[A04_TOUCH_FEATURE_SIZE];
    struct a04_touch_report_state state = {0};
    size_t length = 99;
    memset(descriptor, 0xaa, sizeof(descriptor));
    assert(a04_touch_descriptor(0, 500, descriptor, sizeof(descriptor), &length) != 0);
    assert(length == 99 && descriptor[0] == 0xaa);
    assert(a04_touch_descriptor(700, 500, descriptor, 1, &length) != 0);
    assert(length == 99 && descriptor[0] == 0xaa);
    assert(a04_touch_descriptor(700, 500, descriptor, sizeof(descriptor), &length) == 0);
    assert(length > 80 && length < sizeof(descriptor));
    assert(descriptor[0] == 0x05 && descriptor[1] == 0x0d && descriptor[length-1] == 0xc0);
    assert(a04_touch_report(&state, 0, 10, 20, 0, report) == 0);
    assert(a04_touch_report(&state, 1, 0x1234, 0xabcd, 0xfffe, report) == 1);
    assert(report[0] == 1 && report[1] == 1 && report[2] == 0 &&
           report[3] == 0x34 && report[4] == 0x12 && report[5] == 0xcd &&
           report[6] == 0xab && report[7] == 0xfe && report[8] == 0xff && report[9] == 1);
    assert(a04_touch_report(&state, 0, 999, 999, 1, report) == 1);
    assert(report[1] == 0 && report[3] == 0x34 && report[4] == 0x12 &&
           report[5] == 0xcd && report[6] == 0xab && report[7] == 1 && report[9] == 1);
    assert(a04_touch_report(&state, 0, 0, 0, 2, report) == 0);
    assert(a04_touch_report(&state, 2, 0, 0, 0, report) == -1);
    assert(a04_touch_max_contacts(feature) == 0 && feature[0] == 2 && feature[1] == 1);
    assert(a04_touch_max_contacts(NULL) == -1);
    puts("touch HID descriptor and report tests passed");
    return 0;
}
