/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_MSDC_CDB_H
#define A04BR3_MSDC_CDB_H
#include <stddef.h>
#include <stdint.h>

enum a04_cdb_status {
    A04_CDB_OK = 0,
    A04_CDB_INVALID,
    A04_CDB_UNSUPPORTED,
    A04_CDB_OUT_OF_RANGE
};

struct a04_block_request {
    uint32_t lba;
    uint32_t block_count;
    int write;
};

/* Decode READ/WRITE(10 or 16). Sector-addressed MSDC commands use a 32-bit
 * argument. A zero-length request is a no-op and must not reach hardware.
 * This only validates address and length; the caller must handle CDB flags,
 * transfer buffers, synchronization, FUA and error/sense reporting. */
enum a04_cdb_status a04_decode_block_cdb(const uint8_t *cdb, size_t length,
                                         uint64_t capacity_blocks,
                                         struct a04_block_request *out);
#endif
