/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "scsi_capacity.h"

int a04_scsi_capacity10(uint64_t blocks, uint8_t *out, size_t size)
{
    uint32_t last;
    if (!blocks || !out || size < 8) return 0;
    last = blocks - 1 > UINT32_MAX ? UINT32_MAX : (uint32_t)(blocks - 1);
    out[0] = (uint8_t)(last >> 24);
    out[1] = (uint8_t)(last >> 16);
    out[2] = (uint8_t)(last >> 8);
    out[3] = (uint8_t)last;
    out[4] = 0; out[5] = 0; out[6] = 2; out[7] = 0;
    return 1;
}
