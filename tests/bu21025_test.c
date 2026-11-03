/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "bu21025.h"

struct mock {
    unsigned events[32], count, transfers, fail_at, reads;
};

static int write_byte(void *context, uint8_t value)
{
    struct mock *m = context;
    m->events[m->count++] = 0x100u | value;
    return ++m->transfers == m->fail_at ? -1 : 0;
}

static int read_pair(void *context, uint8_t value[2])
{
    static const uint8_t data[][2] = {{0x12, 0x30}, {0xab, 0xc0}, {0xff, 0xf0}};
    struct mock *m = context;
    m->events[m->count++] = 0x200;
    if (++m->transfers == m->fail_at) return -1;
    assert(m->reads < 3);
    value[0] = data[m->reads][0]; value[1] = data[m->reads++][1];
    return 0;
}

static void delay_us(void *context, unsigned delay)
{
    struct mock *m = context;
    m->events[m->count++] = 0x10000u | delay;
}

int main(void)
{
    static const unsigned expected[] = {
        0x180, 0x10000u | 1500, 0x1c4, 0x200,
        0x190, 0x10000u | 800, 0x1d4, 0x200,
        0x1a0, 0x10000u | 600, 0x1e4, 0x200, 0x100
    };
    struct mock m = {0};
    struct a04_bu21025_io io = {&m, write_byte, read_pair, delay_us};
    struct a04_bu21025_sample sample = {0};
    unsigned i;
    assert(a04_bu21025_init(&io) == 0);
    assert(m.count == 3 && m.events[0] == 0x150 && m.events[1] == 0x1b0 && m.events[2] == 0x100);
    m = (struct mock){0};
    assert(a04_bu21025_sample(&io, &sample) == 0);
    assert(sample.x == 0x123 && sample.y == 0xabc && sample.z == 0xfff);
    assert(m.count == sizeof(expected) / sizeof(expected[0]));
    for (i = 0; i < m.count; ++i) assert(m.events[i] == expected[i]);
    for (i = 1; i <= 10; ++i) {
        m = (struct mock){0}; m.fail_at = i;
        sample = (struct a04_bu21025_sample){1, 2, 3};
        assert(a04_bu21025_sample(&io, &sample) != 0);
        assert(sample.x == 1 && sample.y == 2 && sample.z == 3);
        assert(m.events[m.count - 1] == 0x100);
    }
    assert(a04_bu21025_sample(NULL, &sample) != 0);
    assert(a04_bu21025_sample(&io, NULL) != 0);
    puts("BU21025 sampling tests passed");
    return 0;
}
