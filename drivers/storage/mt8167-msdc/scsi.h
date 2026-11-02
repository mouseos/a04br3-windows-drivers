/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_SCSI_H
#define A04BR3_SCSI_H
#include "cdb.h"

enum a04_scsi_action { A04_SCSI_REPLY, A04_SCSI_READ, A04_SCSI_WRITE, A04_SCSI_ERROR };
struct a04_scsi_reply {
    enum a04_scsi_action action;
    uint32_t transfer_length;
    uint8_t sense_key, asc;
    struct a04_block_request blocks;
};
/* Decode metadata commands and fill their reply. For READ/WRITE, validate the
 * CDB and requested buffer length, then return a block request to the caller.
 * A disabled write path reports DATA PROTECT. No hardware access occurs here. */
struct a04_scsi_reply a04_scsi_process(const uint8_t *cdb, size_t cdb_len,
    uint64_t capacity_blocks, int writable, uint8_t *buffer, size_t buffer_len);
#endif
