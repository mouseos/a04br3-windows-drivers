/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "dma.h"

struct fake { uint32_t regs[64]; int command_error, data_error, no_stop, no_command, fifo_stuck, started; uint32_t first_command, delay, reads, observed; uint64_t time; uint32_t commands[4], command_count; int stop_error, stop_silent, reset_stuck, abort_revoked; unsigned reset_writes, fifo_writes, writes; };
static uint32_t rd(void *p, uint32_t off)
{
    struct fake *f = p;
    if (off == 0x00c && f->started && f->delay && ++f->reads == f->delay)
        f->regs[off / 4] |= 1u << 12;
    return f->regs[off / 4];
}
static void wr(void *p, uint32_t off, uint32_t val)
{
    struct fake *f = p;
    ++f->writes;
    if (off == 0 && (val & (1u<<2))) { ++f->reset_writes; f->regs[0]=f->reset_stuck ? val : val & ~(1u<<2); return; }
    if (off == 0x014 && (val & (1u<<31))) ++f->fifo_writes;
    if (off == 0x00c) f->regs[off / 4] &= ~val;
    else if (off == 0x014) f->regs[off / 4] = f->fifo_stuck ? val : val & ~(1u << 31);
    else {
        f->regs[off / 4] = val;
        if (off == 0x034) {
            if (!f->first_command) f->first_command = val;
            if (f->command_count < 4) f->commands[f->command_count] = val;
            ++f->command_count;
            if ((val & 0x3f) == 12 && f->stop_silent) return;
            if ((val & 0x3f) == 12 && f->stop_error) {
                f->regs[0x00c / 4] |= 1u << 10;
                return;
            }
            if ((val & 0x3f) == 13) f->regs[0x040/4] = (1u << 8) | (4u << 9);
            if (!f->no_command) f->regs[0x00c / 4] |= f->command_error ? 1u << 9 : 1u << 8;
        }
        if (off == 0x098 && (val & 1)) {
            f->started = 1;
            f->regs[0x09c / 4] |= 1;
            if (!f->delay) f->regs[0x00c / 4] |= f->data_error == 3 ? (1u << 19) | (1u << 12) : f->data_error == 2 ? 1u << 14 : f->data_error ? 1u << 15 : 1u << 12;
        }
        if (off == 0x098 && (val & 2) && !f->no_stop) f->regs[0x09c / 4] &= ~1u;
    }
}
static int abort_allowed(void *p) {return !((struct fake *)p)->abort_revoked;}
static uint64_t now(void *p) { struct fake *f = p; return f->time++; }
static void observe(void *p, uint32_t polls) { struct fake *f=p; assert(polls == 4096); ++f->observed; }
static struct a04_msdc_io init(struct fake *f)
{
    struct a04_msdc_io io = {f, rd, 0, wr};
    memset(f, 0, sizeof(*f)); f->regs[0] = 9; f->regs[0x008/4] = 1u << 16;
    return io;
}
int main(void)
{
    struct fake f; struct a04_msdc_io io = init(&f);
    assert(a04_msdc_dma_read_sector(&io, 42, 0x100040, 64, 10) == A04_DMA_OK);
    assert(f.started && f.regs[0x090/4] == 0x100040 && f.regs[0x0a8/4] == 512);
    assert(f.regs[0x038/4] == 42 && f.regs[0] == 9);
    io = init(&f);
    assert(a04_msdc_dma_read_sector(&io, 0, 0x100008, 64, 10) == A04_DMA_INVALID && !f.started);
    assert(a04_msdc_dma_read_sector(&io, 0, 0x100000000ULL, 64, 10) == A04_DMA_INVALID);
    io = init(&f);
    assert(a04_msdc_dma_write_sector(&io, 5, 0x100040, 64, 10, 1) == A04_DMA_OK);
    assert((f.first_command & 0x3f) == 24 && (f.first_command & (1u << 13)));
    io = init(&f); f.regs[0x008/4] = 0;
    assert(a04_msdc_dma_write_sector(&io, 5, 0x100040, 64, 10, 1) == A04_DMA_CARD_BUSY_TIMEOUT);
    io = init(&f); f.command_error = 1;
    assert(a04_msdc_dma_read_sector(&io, 0, 0x100040, 64, 10) == A04_DMA_COMMAND_TIMEOUT && !f.started);
    io = init(&f); f.data_error = 1;
    assert(a04_msdc_dma_read_sector(&io, 0, 0x100040, 64, 10) == A04_DMA_DATA_CRC && f.started);
    io = init(&f); f.no_stop = 1;
    assert(a04_msdc_dma_read_sector(&io, 0, 0x100040, 64, 10) == A04_DMA_STOP_TIMEOUT);
    io = init(&f); f.regs[0x010/4] = 1u << 12;
    assert(a04_msdc_dma_read_sector(&io, 0, 0x100040, 64, 10) == A04_DMA_BUSY && !f.started);
    /* Multi-block CMD18 must be followed by explicit R1b CMD12, including
     * on data errors. No hardware effects on invalid ranges. */
    io = init(&f);
    for (uint32_t sector = 0; sector < 8; ++sector)
        assert(a04_msdc_dma_read_sector(&io, 100 + sector, 0x100000, 64, 10) == A04_DMA_OK);
    assert(f.command_count == 8);
    io = init(&f);
    assert(a04_msdc_dma_read_blocks_timed(&io, 100, 0x100000, 64, 10, 8, 0) == A04_DMA_OK);
    assert(f.command_count == 2 && (f.commands[0] & 63) == 18);
    assert(((f.commands[0] >> 11) & 3) == 2 && (f.commands[1] & 63) == 12);
    assert((f.commands[1] & (1u << 14)) && ((f.commands[1] >> 7) & 7) == 7);
    assert(f.regs[0x050/4] == 8 && f.regs[0x0a8/4] == 4096 && f.regs[0] == 9);
    io = init(&f); f.data_error = 1;
    assert(a04_msdc_dma_read_blocks_timed(&io, 100, 0x100000, 64, 10, 2, 0) == A04_DMA_DATA_CRC);
    assert(f.command_count == 2 && (f.commands[1] & 63) == 12 && f.regs[0] != 9);
    io = init(&f); f.data_error = 3;
    assert(a04_msdc_dma_read_blocks_timed(&io, 100, 0x100000, 64, 10, 2, 0) == A04_DMA_BUS_ERROR);
    assert(f.command_count == 2 && (f.commands[1] & 63) == 12 && f.regs[0] != 9);
    io = init(&f); f.regs[0x010/4] = 1u << 19;
    assert(a04_msdc_dma_read_sector(&io, 0, 0x100040, 64, 10) == A04_DMA_BUSY && !f.started);
    io = init(&f); f.stop_error = 1;
    assert(a04_msdc_dma_read_blocks_timed(&io, 100, 0x100000, 64, 10, 2, 0) == A04_DMA_STOP_ERROR);
    io = init(&f); f.regs[0x008/4] = 0;
    assert(a04_msdc_dma_read_blocks_timed(&io, 100, 0x100000, 64, 10, 2, 0) == A04_DMA_STOP_TIMEOUT);
    assert(f.command_count == 2);
    io = init(&f); f.stop_silent = 1;
    assert(a04_msdc_dma_read_blocks_timed(&io, 100, 0x100000, 64, 10, 2, 0) == A04_DMA_STOP_TIMEOUT);
    io = init(&f); f.no_stop = 1;
    assert(a04_msdc_dma_read_blocks_timed(&io, 100, 0x100000, 64, 10, 2, 0) == A04_DMA_STOP_TIMEOUT);
    assert(f.command_count == 1);
    io = init(&f);
    assert(a04_msdc_dma_read_blocks_timed(&io, 0, 0x100000, 64, 10, 0, 0) == A04_DMA_INVALID);
    assert(a04_msdc_dma_read_blocks_timed(&io, 0, 0x100000, 64, 10, 9, 0) == A04_DMA_INVALID);
    assert(a04_msdc_dma_read_blocks_timed(&io, UINT32_MAX, 0x100000, 64, 10, 2, 0) == A04_DMA_INVALID);
    assert(a04_msdc_dma_read_blocks_timed(&io, 0, 0xfffffe00u, 64, 10, 2, 0) == A04_DMA_INVALID);
    assert(!f.command_count);
    {
        struct a04_dma_clock clock = { &f, now, 3, 0 };
        io = init(&f); f.delay = 1500;
        assert(a04_msdc_dma_sector_timed(&io, 0, 0x100040, 64, 5000, 0, 0, &clock) == A04_DMA_OK);
        io = init(&f); f.delay = 99999;
        assert(a04_msdc_dma_sector_timed(&io, 0, 0x100040, 64, 10000, 0, 0, &clock) == A04_DMA_DATA_POLL_TIMEOUT);
        io = init(&f); f.data_error = 2;
        assert(a04_msdc_dma_sector_timed(&io, 0, 0x100040, 64, 10000, 0, 0, &clock) == A04_DMA_DATA_TIMEOUT);
        io = init(&f); f.fifo_stuck = 1;
        assert(a04_msdc_dma_sector_timed(&io, 0, 0x100040, 64, 100000000, 0, 0, &clock) == A04_DMA_BUSY);
        assert(f.time >= 3 && f.time < 10);
        io = init(&f); f.no_command = 1; clock.ticks=6; clock.observe=observe;
        assert(a04_msdc_dma_sector_timed(&io, 0, 0x100040, 64, 100000000, 0, 0, &clock) == A04_DMA_COMMAND_TIMEOUT);
        assert(f.observed == 1);
        clock.ticks=3; clock.observe=0;
        io = init(&f); f.no_command = 1;
        assert(a04_msdc_dma_sector_timed(&io, 0, 0x100040, 64, 100000000, 0, 0, &clock) == A04_DMA_COMMAND_TIMEOUT);
        assert(f.time >= 3 && f.time < 10);
        io = init(&f); f.no_stop = 1;
        assert(a04_msdc_dma_sector_timed(&io, 0, 0x100040, 64, 100000000, 0, 0, &clock) == A04_DMA_STOP_TIMEOUT);
        assert(f.time >= 3 && f.time < 10);
    }
    {
        struct a04_dma_clock clock={&f,now,20,0};
        io=init(&f);f.regs[0x09c/4]=1;
        assert(a04_msdc_abort_read(&io,20,&clock,abort_allowed,&f)==A04_ABORT_STOPPED);
        assert(!(f.regs[0x09c/4]&1) && f.reset_writes==1 && f.fifo_writes==1 && !f.command_count);
        io=init(&f);f.regs[0x09c/4]=1;f.no_stop=1;
        assert(a04_msdc_abort_read(&io,20,&clock,abort_allowed,&f)==A04_ABORT_STOP_TIMEOUT);
        assert(!f.reset_writes && !f.fifo_writes && !f.command_count);
        io=init(&f);f.reset_stuck=1;
        assert(a04_msdc_abort_read(&io,20,&clock,abort_allowed,&f)==A04_ABORT_RESET_TIMEOUT);
        assert(f.reset_writes==1 && !f.fifo_writes);
        io=init(&f);f.fifo_stuck=1;
        assert(a04_msdc_abort_read(&io,20,&clock,abort_allowed,&f)==A04_ABORT_FIFO_TIMEOUT);
        io=init(&f);f.abort_revoked=1;
        assert(a04_msdc_abort_read(&io,20,&clock,abort_allowed,&f)==A04_ABORT_REVOKED && !f.writes);
        io=init(&f);f.regs[0x098/4]=UINT32_MAX;
        assert(a04_msdc_abort_read(&io,20,&clock,abort_allowed,&f)==A04_ABORT_UNREADABLE && !f.writes);
    }
    return 0;
}
