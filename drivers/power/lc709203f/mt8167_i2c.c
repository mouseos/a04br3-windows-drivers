/* SPDX-License-Identifier: MIT
 * Repeated-start register sequence based on UniXNU A04BR3GaugeCore.h.
 */
#include "mt8167_i2c.h"

int a04_gauge_mt8167_read(void *context, uint8_t address, uint8_t command,
                          uint8_t response[3])
{
    struct a04_gauge_mt8167 *adapter = context;
    const struct a04_i2c_io *io;
    uint8_t data[3];
    uint16_t status = 0;
    uint64_t start, now;
    unsigned n, i;
    if (!adapter || !(io = adapter->io) || !io->read16 || !io->write16 ||
        !io->delay_us || !adapter->monotonic_ns || !response || address != 0x0b || adapter->faulted)
        return -1;
    /* Caller owns I2C2 and holds its shared controller lock throughout. */
    adapter->fault_command = command;
    adapter->fault_reason = 0;
    adapter->fault_status = 0;
    adapter->fault_fifo = 0;
    if (io->read16(io->context, 0x24) & 1u) { adapter->fault_reason = 1; adapter->faulted = 1; return -1; }
    io->write16(io->context, 0x08, 0);
    io->write16(io->context, 0x0c, 7);
    io->write16(io->context, 0x38, 1);
    io->write16(io->context, 0x10,
                (uint16_t)((io->read16(io->context, 0x10) & ~0x3eu) | 0x3au));
    io->write16(io->context, 0x04, (uint16_t)(address << 1));
    io->write16(io->context, 0x14, 1);
    io->write16(io->context, 0x6c, 3);
    io->write16(io->context, 0x18, 2);
    io->write16(io->context, 0x00, command);
    io->write16(io->context, 0x24, 1);
    start = adapter->monotonic_ns(io->context);
    for (n = 0; n < 2000; ++n) {
        now = adapter->monotonic_ns(io->context);
        if (now < start || now - start >= 10000000u) break;
        status = io->read16(io->context, 0x0c);
        now = adapter->monotonic_ns(io->context);
        if (now < start || now - start >= 10000000u) break;
        if (status & 6u) break;
        if (status & 1u) {
            if (((io->read16(io->context, 0x30) >> 4) & 15u) != 3u) break;
            for (i = 0; i < 3; ++i) data[i] = (uint8_t)io->read16(io->context, 0x00);
            io->write16(io->context, 0x0c, 7);
            for (i = 0; i < 3; ++i) response[i] = data[i];
            return 0;
        }
        io->delay_us(io->context, 5);
    }
    adapter->fault_status = status;
    adapter->fault_fifo = io->read16(io->context, 0x30);
    adapter->fault_reason = (status & 6u) ? 2 : ((status & 1u) ? 3 : 4);
    io->write16(io->context, 0x0c, 7);
    /* No reset of a shared controller. The owner must recover the bus. */
    adapter->faulted = 1;
    return -1;
}
