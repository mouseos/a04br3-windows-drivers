/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <stdio.h>
#include "touch_hid.h"
int main(void)
{
    unsigned char data[A04_TOUCH_DESCRIPTOR_CAPACITY];
    size_t length = 0;
    if (a04_touch_descriptor(700, 500, data, sizeof(data), &length)) return 1;
    return fwrite(data, 1, length, stdout) == length ? 0 : 1;
}
