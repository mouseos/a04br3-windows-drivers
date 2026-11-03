/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "pio.h"

struct mock {
    unsigned offsets[32]; uint16_t values[32]; unsigned writes;
    uint16_t status, data[2]; unsigned reads, delays;
};

static uint16_t read16(void *context, unsigned offset)
{
    struct mock *m = context;
    if (offset == 0x0c) return m->status;
    assert(offset == 0 && m->reads < 2);
    return m->data[m->reads++];
}

static void write16(void *context, unsigned offset, uint16_t value)
{
    struct mock *m = context;
    assert(m->writes < 32);
    m->offsets[m->writes] = offset; m->values[m->writes++] = value;
}

static void delay_us(void *context, unsigned delay)
{
    struct mock *m = context;
    assert(delay == 5); ++m->delays;
}

int main(void)
{
    struct mock m = {0};
    struct a04_i2c_io io = {&m, read16, write16, delay_us};
    uint8_t bytes[2] = {0x12, 0x34};
    m.status = 1;
    assert(a04_i2c_transfer(&io, 0x48, 0, bytes, 2) == A04_I2C_OK);
    assert(m.offsets[4] == 0x04 && m.values[4] == 0x90);
    assert(m.offsets[8] == 0x00 && m.values[8] == 0x12);
    assert(m.offsets[9] == 0x00 && m.values[9] == 0x34);
    assert(m.offsets[10] == 0x24 && m.values[10] == 1);
    assert(m.offsets[11] == 0x0c && m.values[11] == 7);
    m = (struct mock){0}; m.status = 1; m.data[0] = 0xab; m.data[1] = 0xcd;
    assert(a04_i2c_transfer(&io, 0x48, 1, bytes, 2) == A04_I2C_OK);
    assert(m.values[4] == 0x91 && bytes[0] == 0xab && bytes[1] == 0xcd);
    m = (struct mock){0}; m.status = 2; bytes[0] = 0x12; bytes[1] = 0x34;
    assert(a04_i2c_transfer(&io, 0x48, 1, bytes, 2) == A04_I2C_BUS_ERROR);
    assert(bytes[0] == 0x12 && bytes[1] == 0x34 && m.reads == 0);
    m = (struct mock){0};
    assert(a04_i2c_transfer(&io, 0x48, 1, bytes, 2) == A04_I2C_TIMEOUT);
    assert(m.delays == 2000 && m.reads == 0 && bytes[0] == 0x12 && bytes[1] == 0x34);
    assert(a04_i2c_transfer(&io, 0x80, 0, bytes, 1) == A04_I2C_INVALID);
    assert(a04_i2c_transfer(&io, 0x48, 0, bytes, 3) == A04_I2C_INVALID);
    puts("MT8167 I2C PIO tests passed");
    return 0;
}
