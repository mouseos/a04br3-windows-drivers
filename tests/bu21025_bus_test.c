/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "mt8167_adapter.h"

struct mock { unsigned transfers, fail_at, fifo, writes; };
static uint16_t read16(void *context, unsigned offset)
{
    static const uint8_t data[] = {0x12,0x30,0xab,0xc0,0xff,0xf0};
    struct mock *m = context;
    if (offset == 0x0c) return m->transfers == m->fail_at ? 2 : 1;
    assert(offset == 0 && m->fifo < sizeof(data));
    return data[m->fifo++];
}
static void write16(void *context, unsigned offset, uint16_t value)
{
    struct mock *m = context;
    (void)value; ++m->writes;
    if (offset == 0x24) ++m->transfers;
}
static void delay_us(void *context, unsigned delay)
{
    (void)context; (void)delay;
}
int main(void)
{
    struct mock m = {0};
    struct a04_i2c_io bus = {&m, read16, write16, delay_us};
    struct a04_bu21025_mt8167 adapter = {&bus, 0x48, 0};
    struct a04_bu21025_io io;
    struct a04_bu21025_sample sample = {0};
    unsigned writes;
    assert(a04_bu21025_mt8167_bind(&adapter, &io) == 0);
    assert(a04_bu21025_sample(&io, &sample) == 0);
    assert(m.transfers == 10 && sample.x == 0x123 && sample.y == 0xabc && sample.z == 0xfff);
    m = (struct mock){0}; m.fail_at = 3;
    sample = (struct a04_bu21025_sample){1,2,3};
    assert(a04_bu21025_sample(&io, &sample) != 0);
    assert(adapter.faulted && m.transfers == 3 && sample.x == 1 && sample.y == 2 && sample.z == 3);
    writes = m.writes;
    assert(a04_bu21025_sample(&io, &sample) != 0 && m.writes == writes);
    assert(a04_bu21025_mt8167_bind(&adapter, &io) != 0);
    puts("BU21025 and MT8167 I2C integration tests passed");
    return 0;
}
