/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "cdb.h"

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static uint64_t be64(const uint8_t *p)
{
    return ((uint64_t)be32(p) << 32) | be32(p + 4);
}

enum a04_cdb_status a04_decode_block_cdb(const uint8_t *cdb, size_t length,
                                         uint64_t capacity_blocks,
                                         struct a04_block_request *out)
{
    uint64_t lba;
    uint32_t count;
    int write;
    if (!cdb || !out || length == 0)
        return A04_CDB_INVALID;
    switch (cdb[0]) {
    case 0x28: case 0x2a:
        if (length < 10) return A04_CDB_INVALID;
        lba = be32(cdb + 2);
        count = ((uint32_t)cdb[7] << 8) | cdb[8];
        write = (cdb[0] == 0x2a);
        break;
    case 0x88: case 0x8a:
        if (length < 16) return A04_CDB_INVALID;
        lba = be64(cdb + 2);
        count = be32(cdb + 10);
        write = (cdb[0] == 0x8a);
        break;
    default:
        return A04_CDB_UNSUPPORTED;
    }
    if (count != 0 && (lba >= capacity_blocks ||
        (uint64_t)count > capacity_blocks - lba ||
        lba > UINT32_MAX || (uint64_t)(count - 1) > UINT32_MAX - lba))
        return A04_CDB_OUT_OF_RANGE;
    if (lba > UINT32_MAX)
        return A04_CDB_OUT_OF_RANGE;
    out->lba = (uint32_t)lba;
    out->block_count = count;
    out->write = write;
    return A04_CDB_OK;
}
