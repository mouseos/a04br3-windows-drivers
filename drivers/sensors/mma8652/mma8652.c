/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "mma8652.h"

enum { ADDRESS = 0x1d, WHO_AM_I = 0x0d, ID = 0x4a, STATUS = 0x00 };

static int16_t decode(const uint8_t *p)
{
    unsigned v = (((unsigned)p[0] << 8) | p[1]) >> 4;
    return (int16_t)((int)v - ((v & 0x800u) ? 4096 : 0));
}
int a04_mma8652_probe(const struct a04_mma8652_bus *bus)
{
    uint8_t id;
    if (!bus || !bus->read_regs) return A04_MMA_INVALID;
    if (bus->read_regs(bus->context, ADDRESS, WHO_AM_I, &id, 1)) return A04_MMA_IO;
    return id == ID ? A04_MMA_OK : A04_MMA_ID;
}
int a04_mma8652_read(const struct a04_mma8652_bus *bus, struct a04_mma8652_sample *out)
{
    uint8_t bytes[7];
    struct a04_mma8652_sample sample;
    if (!bus || !bus->read_regs || !out) return A04_MMA_INVALID;
    /* Status and all axes are captured in the same sequential transaction. */
    if (bus->read_regs(bus->context, ADDRESS, STATUS, bytes, sizeof(bytes))) return A04_MMA_IO;
    if (!(bytes[0] & 0x08u)) return A04_MMA_NOT_READY;
    /* Stock mount direction 7: board x=-sensor y, y=-sensor x, z=-sensor z. */
    sample.x = (int16_t)-decode(bytes + 3);
    sample.y = (int16_t)-decode(bytes + 1);
    sample.z = (int16_t)-decode(bytes + 5);
    *out = sample;
    return A04_MMA_OK;
}

int a04_mma8652_start(const struct a04_mma8652_bus *bus)
{
    uint8_t ctrl, cfg, verify;
    int result = a04_mma8652_probe(bus);
    if (result != A04_MMA_OK) return result;
    if (!bus->write_reg) return A04_MMA_INVALID;
    if (bus->read_regs(bus->context, ADDRESS, 0x2a, &ctrl, 1) ||
        bus->write_reg(bus->context, ADDRESS, 0x2a, (uint8_t)(ctrl & ~1u)) ||
        bus->read_regs(bus->context, ADDRESS, 0x0e, &cfg, 1) ||
        bus->write_reg(bus->context, ADDRESS, 0x0e, (uint8_t)(cfg & ~0x13u)) ||
        bus->read_regs(bus->context, ADDRESS, 0x0e, &verify, 1)) return A04_MMA_IO;
    if (verify & 0x13u) return A04_MMA_IO;
    if (bus->write_reg(bus->context, ADDRESS, 0x2a, (uint8_t)((ctrl & ~0x3bu) | 0x21u)) ||
        bus->read_regs(bus->context, ADDRESS, 0x2a, &verify, 1)) return A04_MMA_IO;
    return (verify & 0x3bu) == 0x21u ? A04_MMA_OK : A04_MMA_IO;
}
