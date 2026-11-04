/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_SYNC_PIO_H
#define A04_SYNC_PIO_H
#include "pio.h"
/* Bounded, synchronous read-only experiment. Caller owns controller exclusively,
 * masks its events and provides a private 4096-byte staging buffer. No DMA is
 * started and no card data is written. Failure poisons controller/card state;
 * restart or resource release is not certified by this routine. */
enum a04_sync_result { A04_SYNC_OK, A04_SYNC_INVALID, A04_SYNC_BUSY,
    A04_SYNC_TIMEOUT, A04_SYNC_COMMAND_ERROR, A04_SYNC_DATA_ERROR,
    A04_SYNC_STOP_ERROR };
struct a04_sync_clock { void *context; uint64_t (*now)(void *); uint64_t ticks; };
enum a04_sync_result a04_sync_pio_read(const struct a04_msdc_io *io,
    uint32_t lba, uint32_t blocks, uint8_t *staging, size_t staging_len,
    uint8_t *output, size_t output_len, uint32_t poll_limit,
    const struct a04_sync_clock *clock);
#endif
