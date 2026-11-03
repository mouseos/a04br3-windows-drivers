/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "mt8167_adapter.h"

static int transfer(struct a04_bu21025_mt8167 *adapter, int read,
                    uint8_t *data, unsigned size)
{
    enum a04_i2c_result status;
    if (adapter->faulted) return -1;
    status = a04_i2c_transfer(adapter->bus, adapter->address, read, data, size);
    if (status != A04_I2C_OK) { adapter->faulted = 1; return -1; }
    return 0;
}

static int write_byte(void *context, uint8_t value)
{
    return transfer((struct a04_bu21025_mt8167 *)context, 0, &value, 1);
}

static int read_pair(void *context, uint8_t value[2])
{
    return transfer((struct a04_bu21025_mt8167 *)context, 1, value, 2);
}

static void delay_us(void *context, unsigned microseconds)
{
    struct a04_bu21025_mt8167 *adapter = context;
    adapter->bus->delay_us(adapter->bus->context, microseconds);
}

int a04_bu21025_mt8167_bind(struct a04_bu21025_mt8167 *adapter,
                            struct a04_bu21025_io *io)
{
    if (!adapter || !io || !adapter->bus || !adapter->bus->read16 ||
        !adapter->bus->write16 || !adapter->bus->delay_us ||
        adapter->address > 0x7f || adapter->faulted) return -1;
    io->context = adapter;
    io->write_byte = write_byte;
    io->read_pair = read_pair;
    io->delay_us = delay_us;
    return 0;
}
