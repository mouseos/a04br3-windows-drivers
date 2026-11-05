/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include "gauge.h"

struct fixture { unsigned calls; int corrupt; uint16_t voltage, indicator, mode; };
static int read_sample(void *context, uint8_t address, uint8_t reg, uint8_t out[3])
{
    struct fixture *f = context;
    uint16_t value;
    uint8_t frame[5], crc = 0;
    unsigned i, j;
    assert(address == 0x0b);
    ++f->calls;
    if (reg == 0x15) value = f->mode;
    else if (reg == 0x09) value = f->voltage;
    else { assert(reg == 0x0f); value = f->indicator; }
    out[0] = (uint8_t)value; out[1] = (uint8_t)(value >> 8);
    frame[0] = 0x16; frame[1] = reg; frame[2] = 0x17;
    frame[3] = out[0]; frame[4] = out[1];
    for (i = 0; i < 5; ++i) {
        crc ^= frame[i];
        for (j = 0; j < 8; ++j) crc = (uint8_t)((crc & 0x80) ? ((crc << 1) ^ 0x07) : crc << 1);
    }
    out[2] = (uint8_t)(crc ^ ((f->corrupt && reg == 0x0f) ? 1 : 0));
    return 0;
}
int main(void)
{
    struct fixture f = {0, 0, 3800, 525, 1};
    struct a04_gauge_bus bus = {&f, read_sample};
    struct a04_gauge_sample sample = {1, 2, 3};
    assert(!a04_gauge_read_sample(&bus, &sample));
    assert(sample.voltage_mv == 3800 && sample.percent == 50 && f.calls == 3);
    f.corrupt = 1;
    assert(a04_gauge_read_sample(&bus, &sample)==A04_GAUGE_INVALID);
    assert(sample.voltage_mv == 3800 && sample.percent == 50);
    f.corrupt = 0; f.indicator = 100; assert(!a04_gauge_read_sample(&bus, &sample)); assert(sample.percent == 0);
    f.indicator = 950; assert(!a04_gauge_read_sample(&bus, &sample)); assert(sample.percent == 100);
    f.mode = 2; assert(a04_gauge_read_sample(&bus, &sample)==A04_GAUGE_INVALID); assert(sample.percent == 100);
    f.mode = 1; f.voltage = 2000; assert(a04_gauge_read_sample(&bus, &sample)==A04_GAUGE_INVALID); assert(sample.percent == 100);
    return 0;
}
