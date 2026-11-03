/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Register sequence based on mouseos/UniXNU-a04br3 A04BR3Input.cpp.
 * Platform-specific ownership and initialization are deliberately external.
 */
#include "pio.h"

enum a04_i2c_result a04_i2c_transfer(const struct a04_i2c_io *io,
                                     uint8_t address, int read,
                                     uint8_t *buffer, unsigned length)
{
    uint8_t result[2];
    unsigned i, n;
    uint16_t status;
    if (!io || !io->read16 || !io->write16 || !io->delay_us || !buffer ||
        address > 0x7f || (read != 0 && read != 1) || length == 0 || length > 2)
        return A04_I2C_INVALID;
    io->write16(io->context, 0x0c, 7);    /* clear pending status */
    io->write16(io->context, 0x38, 1);    /* clear FIFO */
    io->write16(io->context, 0x08, 0);    /* polling mode */
    io->write16(io->context, 0x10, 0x28); /* clock extension, PIO */
    io->write16(io->context, 0x04, (uint16_t)((address << 1) | read));
    io->write16(io->context, 0x14, (uint16_t)length);
    io->write16(io->context, 0x18, 1);
    io->write16(io->context, 0x1c, 2);
    if (!read)
        for (i = 0; i < length; ++i) io->write16(io->context, 0x00, buffer[i]);
    io->write16(io->context, 0x24, 1);
    for (n = 0; n < 2000; ++n) {
        status = io->read16(io->context, 0x0c);
        if (status & 6) {
            io->write16(io->context, 0x0c, 7);
            return A04_I2C_BUS_ERROR;
        }
        if (status & 1) {
            if (read)
                for (i = 0; i < length; ++i) result[i] = (uint8_t)io->read16(io->context, 0x00);
            io->write16(io->context, 0x0c, 7);
            if (read)
                for (i = 0; i < length; ++i) buffer[i] = result[i];
            return A04_I2C_OK;
        }
        io->delay_us(io->context, 5);
    }
    return A04_I2C_TIMEOUT;
}
