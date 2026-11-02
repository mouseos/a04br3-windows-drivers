/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_SCSI_CAPACITY_H
#define A04BR3_SCSI_CAPACITY_H
#include <stddef.h>
#include <stdint.h>

/* Encode a READ CAPACITY (10) reply. Caller handles CDB validation, sense,
 * transfer lengths and the lifetime/locking of the capacity measurement. */
int a04_scsi_capacity10(uint64_t blocks, uint8_t *out, size_t size);
#endif
