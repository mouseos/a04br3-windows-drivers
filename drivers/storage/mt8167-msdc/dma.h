/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04BR3_MSDC_DMA_H
#define A04BR3_MSDC_DMA_H
#include "pio.h"

/* Experimental single-sector read using basic DMA. Caller must own the
 * selected card/controller, mask interrupts, supply an OS-mapped DMA bus
 * address, and handle cache synchronization and buffer lifetime. The whole
 * 512-byte buffer must be DMA-accessible, contiguous and below 4 GiB.
 * Failure leaves the controller untrusted: quiesce/reinitialize before reuse.
 * This interface does not perform Windows DMA mapping or SCSI handling. */
enum a04_dma_result {
    A04_DMA_OK = 0, A04_DMA_INVALID, A04_DMA_BUSY,
    A04_DMA_COMMAND_TIMEOUT, A04_DMA_COMMAND_CRC, A04_DMA_CARD_ERROR,
    A04_DMA_DATA_TIMEOUT, A04_DMA_DATA_CRC, A04_DMA_STOP_TIMEOUT
};
enum a04_dma_result a04_msdc_dma_read_sector(const struct a04_msdc_io *io,
        uint32_t lba, uint64_t bus_address, uint32_t alignment, uint32_t poll_limit);
#endif
