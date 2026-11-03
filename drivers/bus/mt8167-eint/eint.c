/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Register layout and polarity/sensitivity semantics based on Linux upstream
 * drivers/pinctrl/mediatek/mtk-eint.c and pinctrl-mt8167.c; independently written.
 */
#include "eint.h"
static int valid(const struct a04_eint_io *io, unsigned line)
{
    return io && io->read32 && io->write32 && line < A04_EINT_LINES;
}
static unsigned offset(unsigned base, unsigned line) { return base + (line / 32u) * 4u; }
static uint32_t bit(unsigned line) { return 1u << (line % 32u); }

int a04_eint_mask(const struct a04_eint_io *io, unsigned line, int masked)
{
    if (!valid(io, line) || (masked != 0 && masked != 1)) return -1;
    io->write32(io->context, offset(masked ? 0x0c0u : 0x100u, line), bit(line));
    return 0;
}
int a04_eint_ack(const struct a04_eint_io *io, unsigned line)
{
    if (!valid(io, line)) return -1;
    io->write32(io->context, offset(0x040u, line), bit(line));
    return 0;
}
int a04_eint_configure(const struct a04_eint_io *io, unsigned line, enum a04_eint_type type)
{
    uint32_t mask; int was_masked;
    if (!valid(io,line) || type < A04_EDGE_RISING || type > A04_LEVEL_LOW) return -1;
    mask=bit(line);
    was_masked=(io->read32(io->context,offset(0x080u,line)) & mask) != 0;
    if (!was_masked) io->write32(io->context,offset(0x0c0u,line),mask);
    io->write32(io->context,offset((type==A04_EDGE_FALLING || type==A04_LEVEL_LOW) ? 0x380u : 0x340u,line),mask);
    io->write32(io->context,offset((type==A04_EDGE_RISING || type==A04_EDGE_FALLING) ? 0x1c0u : 0x180u,line),mask);
    io->write32(io->context,offset(0x040u,line),mask);
    if (!was_masked) io->write32(io->context,offset(0x100u,line),mask);
    return 0;
}
int a04_eint_pending(const struct a04_eint_io *io, unsigned bank,
                     uint32_t owned, uint32_t *pending)
{
    uint32_t valid_bits;
    if (!io || !io->read32 || !io->write32 || !pending || bank >= A04_EINT_BANKS) return -1;
    valid_bits = bank == 5 ? 0x1ffu : 0xffffffffu;
    *pending = io->read32(io->context,bank*4u) &
               ~io->read32(io->context,0x080u+bank*4u) & owned & valid_bits;
    return 0;
}
