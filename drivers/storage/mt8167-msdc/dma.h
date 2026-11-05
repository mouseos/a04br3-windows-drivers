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
    A04_DMA_DATA_TIMEOUT, A04_DMA_DATA_CRC, A04_DMA_STOP_TIMEOUT,
    A04_DMA_CARD_BUSY_TIMEOUT, A04_DMA_DATA_POLL_TIMEOUT, A04_DMA_STOP_ERROR,
    A04_DMA_BUS_ERROR
};
struct a04_dma_clock {
    void *context;
    uint64_t (*now)(void *context);
    uint64_t ticks;
    void (*observe)(void *context, uint32_t polls);
};
enum a04_dma_result a04_msdc_dma_sector_timed(const struct a04_msdc_io *io,
        uint32_t lba, uint64_t bus_address, uint32_t alignment, uint32_t poll_limit,
        int write, uint16_t rca, const struct a04_dma_clock *clock);
/* Experimental CMD18 + CMD12 read, 1..8 contiguous sectors. On any error
 * the controller and card are untrusted; do not retry without reinitializing. */
enum a04_dma_result a04_msdc_dma_read_blocks_timed(const struct a04_msdc_io *io,
        uint32_t lba, uint64_t bus_address, uint32_t alignment, uint32_t poll_limit,
        uint32_t blocks, const struct a04_dma_clock *clock);
/* Read-error recovery only, while the caller exclusively owns the controller.
 * Success proves only that the controller reported DMA inactive around a
 * completed host reset/FIFO clear; it does not prove card state or allow I/O.
 * A reset after STOP timeout is never proof of bus DMA drain, even if the
 * engine reports idle: return RESET_UNPROVEN and retain DMA resources.
 * Never use this to retry a write. If access is revoked or any step fails,
 * retain DMA resources and treat the device as untrusted. */
enum a04_dma_abort_result { A04_ABORT_STOPPED, A04_ABORT_INVALID,
    A04_ABORT_REVOKED, A04_ABORT_STOP_TIMEOUT, A04_ABORT_RESET_TIMEOUT,
    A04_ABORT_FIFO_TIMEOUT, A04_ABORT_UNREADABLE, A04_ABORT_RESET_UNPROVEN };
enum a04_dma_abort_result a04_msdc_abort_read(const struct a04_msdc_io *,
    uint32_t poll_limit,const struct a04_dma_clock *,int (*allowed)(void *),void *context);
enum a04_dma_result a04_msdc_dma_read_sector(const struct a04_msdc_io *io,
        uint32_t lba, uint64_t bus_address, uint32_t alignment, uint32_t poll_limit);
/* Write is experimental and requires an OS-owned, cache-synchronized DMA
 * buffer plus a caller-enforced cache/flush policy. A timeout or error leaves
 * on-media contents indeterminate; never automatically retry a write. */
enum a04_dma_result a04_msdc_dma_write_sector(const struct a04_msdc_io *io,
        uint32_t lba, uint64_t bus_address, uint32_t alignment, uint32_t poll_limit, uint16_t rca);
#endif
