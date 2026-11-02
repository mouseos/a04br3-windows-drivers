/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "scsi_capacity.h"
int main(void)
{
    uint8_t b[8] = {0};
    const uint8_t expected[] = {1, 0xd1, 0xef, 0xff, 0, 0, 2, 0};
    const uint8_t saturated[] = {0xff, 0xff, 0xff, 0xff, 0, 0, 2, 0};
    assert(!a04_scsi_capacity10(0, b, sizeof b));
    assert(!a04_scsi_capacity10(1, b, 7));
    assert(!a04_scsi_capacity10(1, 0, 8));
    assert(a04_scsi_capacity10(0x01d1f000, b, sizeof b));
    assert(!memcmp(b, expected, sizeof b));
    assert(a04_scsi_capacity10(UINT64_MAX, b, sizeof b));
    assert(!memcmp(b, saturated, sizeof b));
    return 0;
}
