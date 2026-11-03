/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 GPIO and I2C0 setup, derived from UniXNU-a04br3 A04BR3Input.cpp.
 */
#include "board_init.h"

int a04_touch_i2c_reset(const struct a04_touch_board_io *io)
{
    if (!io || !io->write_i2c16) return -1;
    if (io->write_i2c16(io->context, 0x50, 1) ||
        io->write_i2c16(io->context, 0x40, 3) ||
        io->write_i2c16(io->context, 0x54, 0) ||
        io->write_i2c16(io->context, 0x70, 4) ||
        io->write_i2c16(io->context, 0x20, 0x73f) ||
        io->write_i2c16(io->context, 0x48, 0) ||
        io->write_i2c16(io->context, 0x28, 0x8001)) return -1;
    return 0;
}

int a04_touch_board_init(const struct a04_touch_board_io *io)
{
    if (!io || !io->update32 || !io->write32 || !io->write_i2c16) return -1;
    /* GPIO58/59 I2C function, GPIO8 input. */
    if (io->update32(io->context, A04_TOUCH_GPIO, 0x3b0, 7u << 9, 1u << 9) ||
        io->update32(io->context, A04_TOUCH_GPIO, 0x3b0, 7u << 12, 1u << 12) ||
        io->update32(io->context, A04_TOUCH_GPIO, 0x310, 7u << 9, 0) ||
        io->write32(io->context, A04_TOUCH_GPIO, 0x08, 1u << 8) ||
        io->write32(io->context, A04_TOUCH_GPIO, 0x904, (1u << 3) | (1u << 15)) ||
        io->write32(io->context, A04_TOUCH_TOP, 0x84, 1u << 3) ||
        io->update32(io->context, A04_TOUCH_INFRA, 0x80, 1u << 1, 0)) return -1;
    return a04_touch_i2c_reset(io);
}
