/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "dma.h"
#define CFG 0x000u
#define PIN_STATUS 0x008u
#define DAT0_HIGH (1u << 16)
#define INT_STATUS 0x00cu
#define INT_ENABLE 0x010u
#define FIFO_STATUS 0x014u
#define COMMAND 0x034u
#define ARGUMENT 0x038u
#define STATUS 0x03cu
#define RESPONSE0 0x040u
#define BLOCK_COUNT 0x050u
#define DMA_HIGH 0x08cu
#define DMA_ADDRESS 0x090u
#define DMA_CONTROL 0x098u
#define DMA_CONFIG 0x09cu
#define DMA_LENGTH 0x0a8u
#define CFG_PIO (1u << 3)
#define FIFO_CLEAR (1u << 31)
#define CMD_READY (1u << 8)
#define CMD_TIMEOUT (1u << 9)
#define CMD_CRC (1u << 10)
#define XFER_DONE (1u << 12)
#define DATA_TIMEOUT (1u << 14)
#define DATA_CRC (1u << 15)
#define EVENTS (CMD_READY | CMD_TIMEOUT | CMD_CRC | XFER_DONE | DATA_TIMEOUT | DATA_CRC)
#define DMA_START 1u
#define DMA_STOP 2u
#define DMA_MODE (1u << 8)
#define DMA_LAST (1u << 10)
#define DMA_BURST (7u << 12)
#define DMA_ACTIVE 1u

/* Bound every MMIO polling phase by both iterations and a monotonic clock.
 * The clock must advance even when the caller runs above timer interrupt IRQL. */
static int a04_dma_expired(const struct a04_dma_clock *clock, uint64_t start, uint32_t n)
{
    if (clock && clock->observe && (n & 4095u)==4095u)
        clock->observe(clock->context,n+1);
    return clock && !(n & 1023u) && clock->now(clock->context) - start >= clock->ticks;
}

/* Register meanings and command order checked against the platform's Linux
 * MT8167 host driver; this implementation is independently written. */
static enum a04_dma_result a04_dma_transfer(const struct a04_msdc_io *io,
        uint32_t lba, uint64_t bus_address, uint32_t alignment, uint32_t limit, int write, uint16_t rca, uint32_t blocks,
        const struct a04_dma_clock *clock)
{
    uint32_t cfg, ctrl, n, bits;
    uint64_t start = 0;
    enum a04_dma_result result = A04_DMA_OK;
    if (!io || !io->read32 || !io->write32 || !limit || !alignment ||
        (alignment & (alignment - 1)) || (bus_address & (alignment - 1)) ||
        !blocks || blocks > 8 || (write && blocks != 1) ||
        lba > UINT32_MAX - (blocks - 1) ||
        bus_address > UINT32_MAX - (blocks * 512u - 1u) || (write && !rca) ||
        (clock && (!clock->now || !clock->ticks)))
        return A04_DMA_INVALID;
    if ((io->read32(io->context, INT_ENABLE) & EVENTS) ||
        (io->read32(io->context, STATUS) & 1u) ||
        (io->read32(io->context, DMA_CONFIG) & DMA_ACTIVE))
        return A04_DMA_BUSY;
    cfg = io->read32(io->context, CFG);
    ctrl = io->read32(io->context, DMA_CONTROL) & ~(DMA_START | DMA_STOP);
    io->write32(io->context, CFG, cfg & ~CFG_PIO);
    io->write32(io->context, FIFO_STATUS, io->read32(io->context, FIFO_STATUS) | FIFO_CLEAR);
    if (clock) start = clock->now(clock->context);
    for (n = 0; n < limit; ++n) {
        if (!(io->read32(io->context, FIFO_STATUS) & FIFO_CLEAR)) break;
        if (a04_dma_expired(clock,start,n)) return A04_DMA_BUSY;
    }
    if (n == limit) return A04_DMA_BUSY;
    bits = io->read32(io->context, INT_STATUS) & EVENTS;
    if (bits) io->write32(io->context, INT_STATUS, bits);
    io->write32(io->context, BLOCK_COUNT, blocks);
    io->write32(io->context, ARGUMENT, lba);
    io->write32(io->context, COMMAND, (write ? 24u : blocks > 1 ? 18u : 17u) | (1u << 7) |
                ((blocks > 1 ? 2u : 1u) << 11) | (write ? 1u << 13 : 0u) | (512u << 16));
    if (clock) start = clock->now(clock->context);
    for (n = 0; n < limit; ++n) {
        bits = io->read32(io->context, INT_STATUS) & (CMD_READY | CMD_TIMEOUT | CMD_CRC);
        if (!bits) {
            if (a04_dma_expired(clock,start,n)) return A04_DMA_COMMAND_TIMEOUT;
            continue;
        }
        io->write32(io->context, INT_STATUS, bits);
        if (bits & CMD_TIMEOUT) return A04_DMA_COMMAND_TIMEOUT;
        if (bits & CMD_CRC) return A04_DMA_COMMAND_CRC;
        break;
    }
    if (n == limit) return A04_DMA_COMMAND_TIMEOUT;
    if (io->read32(io->context, RESPONSE0) & 0xffff0000u) return A04_DMA_CARD_ERROR;
    io->write32(io->context, DMA_HIGH, 0);
    io->write32(io->context, DMA_ADDRESS, (uint32_t)bus_address);
    io->write32(io->context, DMA_LENGTH, blocks * 512u);
    ctrl = (ctrl & ~(DMA_MODE | DMA_BURST)) | DMA_LAST | (6u << 12);
    io->write32(io->context, DMA_CONTROL, ctrl);
    io->write32(io->context, DMA_CONTROL, ctrl | DMA_START);
    if (clock) start = clock->now(clock->context);
    for (n = 0; n < limit; ++n) {
        bits = io->read32(io->context, INT_STATUS) & (XFER_DONE | DATA_TIMEOUT | DATA_CRC);
        if (!bits) {
            if (a04_dma_expired(clock,start,n))
                break;
            continue;
        }
        io->write32(io->context, INT_STATUS, bits);
        if (bits & DATA_TIMEOUT) result = A04_DMA_DATA_TIMEOUT;
        else if (bits & DATA_CRC) result = A04_DMA_DATA_CRC;
        break;
    }
    if (!bits) result = A04_DMA_DATA_POLL_TIMEOUT;
    io->write32(io->context, DMA_CONTROL, ctrl | DMA_STOP);
    if (clock) start = clock->now(clock->context);
    for (n = 0; n < limit; ++n) {
        if (!(io->read32(io->context, DMA_CONFIG) & DMA_ACTIVE)) break;
        if (a04_dma_expired(clock,start,n)) return A04_DMA_STOP_TIMEOUT;
    }
    if (n == limit) return A04_DMA_STOP_TIMEOUT;
    if (blocks > 1) {
        /* CMD18 is open-ended. Stop it even after a data error, but never
         * treat a failed stop as recoverable or copy partial data to the SRB. */
        if (clock) start = clock->now(clock->context);
        for (n = 0; n < limit; ++n) {
            if (!(io->read32(io->context, STATUS) & 1u)) break;
            if (a04_dma_expired(clock,start,n)) return A04_DMA_STOP_TIMEOUT;
        }
        if (n == limit) return A04_DMA_STOP_TIMEOUT;
        bits = io->read32(io->context, INT_STATUS) & (CMD_READY | CMD_TIMEOUT | CMD_CRC);
        if (bits) io->write32(io->context, INT_STATUS, bits);
        io->write32(io->context, ARGUMENT, 0);
        io->write32(io->context, COMMAND, 12u | (7u << 7) | (1u << 14));
        if (clock) start = clock->now(clock->context);
        for (n = 0; n < limit; ++n) {
            bits = io->read32(io->context, INT_STATUS) & (CMD_READY | CMD_TIMEOUT | CMD_CRC);
            if (bits) break;
            if (a04_dma_expired(clock,start,n)) return A04_DMA_STOP_TIMEOUT;
        }
        if (n == limit) return A04_DMA_STOP_TIMEOUT;
        io->write32(io->context, INT_STATUS, bits);
        if ((bits & (CMD_TIMEOUT | CMD_CRC)) || !(bits & CMD_READY) ||
            (io->read32(io->context, RESPONSE0) & 0xffff0000u)) return A04_DMA_STOP_ERROR;
        if (clock) start = clock->now(clock->context);
        for (n = 0; n < limit; ++n) {
            if (io->read32(io->context, PIN_STATUS) & DAT0_HIGH) break;
            if (a04_dma_expired(clock,start,n)) return A04_DMA_STOP_TIMEOUT;
        }
        if (n == limit) return A04_DMA_STOP_TIMEOUT;
    }
    if (result == A04_DMA_OK && write) {
        /* DAT0 remains low while eMMC is programming the data. */
        if (clock) start = clock->now(clock->context);
        for (n = 0; n < limit; ++n) {
            if (io->read32(io->context, PIN_STATUS) & DAT0_HIGH) break;
            if (a04_dma_expired(clock,start,n)) return A04_DMA_CARD_BUSY_TIMEOUT;
        }
        if (n == limit) return A04_DMA_CARD_BUSY_TIMEOUT;
        /* CMD13: require READY_FOR_DATA and transfer state; failures are not
         * retried because the write may already have reached the card. */
        if (clock) start = clock->now(clock->context);
        for (n = 0; n < limit; ++n) {
            if (!(io->read32(io->context, STATUS) & 1u)) break;
            if (a04_dma_expired(clock,start,n)) return A04_DMA_CARD_BUSY_TIMEOUT;
        }
        if (n == limit) return A04_DMA_CARD_BUSY_TIMEOUT;
        bits = io->read32(io->context, INT_STATUS) & (CMD_READY | CMD_TIMEOUT | CMD_CRC);
        if (bits) io->write32(io->context, INT_STATUS, bits);
        io->write32(io->context, ARGUMENT, (uint32_t)rca << 16);
        io->write32(io->context, COMMAND, 13u | (1u << 7));
        if (clock) start = clock->now(clock->context);
        for (n = 0; n < limit; ++n) {
            bits = io->read32(io->context, INT_STATUS) & (CMD_READY | CMD_TIMEOUT | CMD_CRC);
            if (!bits) {
                if (a04_dma_expired(clock,start,n)) return A04_DMA_COMMAND_TIMEOUT;
                continue;
            }
            io->write32(io->context, INT_STATUS, bits);
            if (bits & CMD_TIMEOUT) return A04_DMA_COMMAND_TIMEOUT;
            if (bits & CMD_CRC) return A04_DMA_COMMAND_CRC;
            break;
        }
        if (n == limit) return A04_DMA_COMMAND_TIMEOUT;
        bits = io->read32(io->context, RESPONSE0);
        if ((bits & 0xffff0000u) || !(bits & (1u << 8)) || ((bits >> 9) & 15u) != 4)
            return A04_DMA_CARD_ERROR;
    }
    if (result == A04_DMA_OK) io->write32(io->context, CFG, cfg);
    return result;
}

enum a04_dma_result a04_msdc_dma_sector_timed(const struct a04_msdc_io *io,
        uint32_t lba, uint64_t bus_address, uint32_t alignment, uint32_t limit, int write, uint16_t rca,
        const struct a04_dma_clock *clock)
{ return a04_dma_transfer(io, lba, bus_address, alignment, limit, write, rca, 1, clock); }

enum a04_dma_result a04_msdc_dma_read_blocks_timed(const struct a04_msdc_io *io,
        uint32_t lba, uint64_t bus_address, uint32_t alignment, uint32_t limit,
        uint32_t blocks, const struct a04_dma_clock *clock)
{ return a04_dma_transfer(io, lba, bus_address, alignment, limit, 0, 0, blocks, clock); }

enum a04_dma_result a04_msdc_dma_read_sector(const struct a04_msdc_io *io,
        uint32_t lba, uint64_t bus_address, uint32_t alignment, uint32_t limit)
{ return a04_msdc_dma_sector_timed(io, lba, bus_address, alignment, limit, 0, 0, 0); }

enum a04_dma_result a04_msdc_dma_write_sector(const struct a04_msdc_io *io,
        uint32_t lba, uint64_t bus_address, uint32_t alignment, uint32_t limit, uint16_t rca)
{ return a04_msdc_dma_sector_timed(io, lba, bus_address, alignment, limit, 1, rca, 0); }
