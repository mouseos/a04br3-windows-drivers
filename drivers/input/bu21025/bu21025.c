/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Command sequence and timing derived from mouseos/UniXNU-a04br3
 * package/A04BR3Input.cpp, itself based on the board's Linux sources.
 */
#include "bu21025.h"

static int valid(const struct a04_bu21025_io *io)
{
    return io && io->write_byte && io->read_pair && io->delay_us;
}

int a04_bu21025_init(const struct a04_bu21025_io *io)
{
    if (!valid(io)) return -1;
    if (io->write_byte(io->context, 0x50) ||
        io->write_byte(io->context, 0xb0) ||
        io->write_byte(io->context, 0x00)) {
        (void)io->write_byte(io->context, 0x00);
        return -1;
    }
    return 0;
}

static int measure(const struct a04_bu21025_io *io, uint8_t select,
                   uint8_t read, unsigned delay, uint16_t *result)
{
    uint8_t data[2];
    if (io->write_byte(io->context, select)) return -1;
    io->delay_us(io->context, delay);
    if (io->write_byte(io->context, read) || io->read_pair(io->context, data)) return -1;
    *result = (uint16_t)(((uint16_t)data[0] << 4) | (data[1] >> 4));
    return 0;
}

int a04_bu21025_sample(const struct a04_bu21025_io *io,
                       struct a04_bu21025_sample *sample)
{
    struct a04_bu21025_sample next;
    int failed, standby;
    if (!valid(io) || !sample) return -1;
    failed = measure(io, 0x80, 0xc4, 1500, &next.x) ||
             measure(io, 0x90, 0xd4, 800, &next.y) ||
             measure(io, 0xa0, 0xe4, 600, &next.z);
    standby = io->write_byte(io->context, 0x00);
    if (failed || standby) return -1;
    *sample = next;
    return 0;
}
