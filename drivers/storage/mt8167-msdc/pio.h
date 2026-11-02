/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_MSDC_PIO_H
#define A04BR3_MSDC_PIO_H
#include <stddef.h>
#include <stdint.h>

/* Caller must own the controller and provide ordered MMIO access. The card
 * must already be selected, in sector-addressed transfer mode, and the host
 * must already be configured for PIO, and all relevant controller interrupts
 * must be masked by the caller. This code does not initialize clocks,
 * power, pins, bus width, interrupts or the card. */
struct a04_msdc_io {
    void *context;
    uint32_t (*read32)(void *context, uint32_t offset);
    uint8_t (*read8)(void *context, uint32_t offset);
    void (*write32)(void *context, uint32_t offset, uint32_t value);
};
enum a04_msdc_result {
    A04_MSDC_OK = 0,
    A04_MSDC_INVALID,
    A04_MSDC_BUSY_TIMEOUT,
    A04_MSDC_FIFO_TIMEOUT,
    A04_MSDC_COMMAND_TIMEOUT,
    A04_MSDC_COMMAND_CRC,
    A04_MSDC_CARD_ERROR,
    A04_MSDC_DATA_TIMEOUT,
    A04_MSDC_DATA_CRC,
    A04_MSDC_DATA_ERROR
};
/* One CMD17, exactly one 512-byte sector, no writes to card data. poll_limit
 * bounds each polling stage but is not a time unit. A failure may leave a
 * partially filled buffer and requires controller recovery before reuse. */
enum a04_msdc_result a04_msdc_read_sector(const struct a04_msdc_io *io,
                                          uint32_t lba, uint8_t *buffer,
                                          size_t length, uint32_t poll_limit);
#endif
