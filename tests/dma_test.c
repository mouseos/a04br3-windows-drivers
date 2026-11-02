/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "dma.h"

struct fake { uint32_t regs[64]; int command_error, data_error, no_stop, started; };
static uint32_t rd(void *p, uint32_t off)
{
    struct fake *f = p;
    return f->regs[off / 4];
}
static void wr(void *p, uint32_t off, uint32_t val)
{
    struct fake *f = p;
    if (off == 0x00c) f->regs[off / 4] &= ~val;
    else if (off == 0x014) f->regs[off / 4] = val & ~(1u << 31);
    else {
        f->regs[off / 4] = val;
        if (off == 0x034) f->regs[0x00c / 4] |= f->command_error ? 1u << 9 : 1u << 8;
        if (off == 0x098 && (val & 1)) {
            f->started = 1;
            f->regs[0x09c / 4] |= 1;
            f->regs[0x00c / 4] |= f->data_error ? 1u << 15 : 1u << 12;
        }
        if (off == 0x098 && (val & 2) && !f->no_stop) f->regs[0x09c / 4] &= ~1u;
    }
}
static struct a04_msdc_io init(struct fake *f)
{
    struct a04_msdc_io io = {f, rd, 0, wr};
    memset(f, 0, sizeof(*f)); f->regs[0] = 9;
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
    io = init(&f); f.command_error = 1;
    assert(a04_msdc_dma_read_sector(&io, 0, 0x100040, 64, 10) == A04_DMA_COMMAND_TIMEOUT && !f.started);
    io = init(&f); f.data_error = 1;
    assert(a04_msdc_dma_read_sector(&io, 0, 0x100040, 64, 10) == A04_DMA_DATA_CRC && f.started);
    io = init(&f); f.no_stop = 1;
    assert(a04_msdc_dma_read_sector(&io, 0, 0x100040, 64, 10) == A04_DMA_STOP_TIMEOUT);
    io = init(&f); f.regs[0x010/4] = 1u << 12;
    assert(a04_msdc_dma_read_sector(&io, 0, 0x100040, 64, 10) == A04_DMA_BUSY && !f.started);
    return 0;
}
