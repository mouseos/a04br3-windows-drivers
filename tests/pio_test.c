/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "pio.h"
#include <assert.h>
#include <string.h>

struct fake {
    uint32_t regs[32], lba, command;
    unsigned consumed, fifo_clears, writes, mode;
};
static uint32_t rd32(void *ctx, uint32_t reg)
{
    struct fake *f = ctx;
    if (reg == 0x3c) return f->mode == 1 ? 1 : 0;
    if (reg == 0x14) {
        if (f->mode == 2) return 1u << 31;
        return f->command && f->consumed < 512 ? 64 : 0;
    }
    if (reg == 0xc && f->command) {
        if (f->mode == 3) return 1u << 9;
        if (f->mode == 4) return (1u << 8) | (1u << 10);
        if (f->mode == 5) return (1u << 8) | (1u << 15);
        return (1u << 8) | (f->consumed == 512 ? 1u << 12 : 0);
    }
    if (reg == 0x40) return f->mode == 6 ? (1u << 31) : 0x900;
    if (reg == 0x1c) {
        uint32_t v = 0; unsigned i;
        for (i = 0; i < 4; ++i) v |= (uint32_t)(uint8_t)f->consumed++ << (i * 8);
        return v;
    }
    return f->regs[reg / 4];
}
static uint8_t rd8(void *ctx, uint32_t reg)
{
    struct fake *f = ctx; assert(reg == 0x1c); return (uint8_t)f->consumed++;
}
static void wr32(void *ctx, uint32_t reg, uint32_t value)
{
    struct fake *f = ctx; ++f->writes;
    if (reg == 0x14) { assert(value & (1u << 31)); ++f->fifo_clears; }
    else if (reg == 0x38) f->lba = value;
    else if (reg == 0x34) f->command = value;
    else if (reg != 0xc) f->regs[reg / 4] = value;
}
int main(void)
{
    struct fake f = {0}; uint8_t data[512]; unsigned i;
    struct a04_msdc_io io = {&f, rd32, rd8, wr32};
    assert(a04_msdc_read_sector(&io, 0x12345678, data, sizeof(data), 100) == A04_MSDC_OK);
    assert(f.lba == 0x12345678 && f.command == (17u | (1u << 7) | (1u << 11) | (512u << 16)));
    assert(f.regs[0x50 / 4] == 1 && f.consumed == 512 && f.fifo_clears == 1);
    for (i = 0; i < sizeof(data); ++i) assert(data[i] == (uint8_t)i);
    memset(&f, 0, sizeof(f));
    assert(a04_msdc_read_ext_csd(&io, data, sizeof(data), 100) == A04_MSDC_OK);
    assert(f.lba == 0 && f.command == (8u | (1u << 7) | (1u << 11) | (512u << 16)));
    for (i = 0; i < sizeof(data); ++i) assert(data[i] == (uint8_t)i);
    for (i = 1; i <= 6; ++i) {
        enum a04_msdc_result expected[] = {A04_MSDC_OK, A04_MSDC_BUSY_TIMEOUT,
            A04_MSDC_FIFO_TIMEOUT, A04_MSDC_COMMAND_TIMEOUT, A04_MSDC_COMMAND_CRC,
            A04_MSDC_DATA_CRC, A04_MSDC_CARD_ERROR};
        memset(&f, 0, sizeof(f)); f.mode = i;
        assert(a04_msdc_read_sector(&io, 7, data, sizeof(data), 100) == expected[i]);
    }
    memset(&f, 0, sizeof(f));
    assert(a04_msdc_read_sector(&io, 0, data, 511, 100) == A04_MSDC_INVALID && !f.writes);
    assert(a04_msdc_read_sector(&io, 0, data, 512, 0) == A04_MSDC_INVALID && !f.writes);
    return 0;
}
