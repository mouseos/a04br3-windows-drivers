/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_SCSI_H
#define A04BR3_SCSI_H
#include "cdb.h"

enum a04_scsi_action { A04_SCSI_REPLY, A04_SCSI_READ, A04_SCSI_WRITE, A04_SCSI_ERROR, A04_SCSI_FLUSH };
struct a04_scsi_reply {
    enum a04_scsi_action action;
    uint32_t transfer_length;
    uint8_t sense_key, asc;
    struct a04_block_request blocks;
};
/* Decode metadata commands and fill their reply. For READ/WRITE, validate the
 * CDB and requested buffer length, then return a block request to the caller.
 * A disabled write path reports DATA PROTECT. No hardware access occurs here.
 * FLUSH is an action, not success: caller must prove the live cache policy
 * and prior I/O completion. MODE SENSE WCE=0 also requires that proof. */
struct a04_scsi_reply a04_scsi_process(const uint8_t *cdb, size_t cdb_len,
    uint64_t capacity_blocks, int writable, uint8_t *buffer, size_t buffer_len);
/* Apply a volatile firmware-supplied eMMC MID and six CID product bytes to a
 * standard SCSI INQUIRY response. Invalid identities leave it unchanged. */
int a04_scsi_set_emmc_identity(uint8_t *inquiry, size_t length, const uint8_t identity[7]);
#endif
