/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include "mt8167_i2c.h"
struct mock { uint16_t regs[64]; unsigned reads, delays; int outcome; };
static uint16_t rd(void *p, unsigned reg)
{
    struct mock *m = p;
    if (reg == 0x24) return m->outcome == 4 ? 1 : 0;
    if (reg == 0x0c) return m->outcome == 1 ? 2 : m->outcome == 2 ? 0 : 1;
    if (reg == 0x30) return m->outcome == 3 ? 0x20 : 0x30;
    if (reg == 0) return (uint16_t)(0xa0 + m->reads++);
    return m->regs[reg / 2];
}
static void wr(void *p, unsigned reg, uint16_t value)
{
    struct mock *m = p; m->regs[reg / 2] = value;
}
static void delay(void *p, unsigned us) { struct mock *m = p; assert(us == 5); ++m->delays; }
static uint64_t now_ns(void *p) { struct mock *m = p; return (uint64_t)m->delays * 5000; }
int main(void)
{
    struct mock m = {{0}, 0, 0, 0};
    struct a04_i2c_io io = {&m, rd, wr, delay};
    struct a04_gauge_mt8167 a = {&io, now_ns, 0};
    uint8_t out[3] = {1, 2, 3};
    assert(!a04_gauge_mt8167_read(&a, 0x0b, 0x09, out));
    assert(out[0] == 0xa0 && out[1] == 0xa1 && out[2] == 0xa2);
    assert(m.regs[0x04 / 2] == 0x16 && m.regs[0x14 / 2] == 1 &&
           m.regs[0x6c / 2] == 3 && m.regs[0x18 / 2] == 2);
    out[0] = 1; m.outcome = 3;
    assert(a04_gauge_mt8167_read(&a, 0x0b, 0x09, out));
    assert(a.faulted && out[0] == 1);
    m.outcome = 0; assert(a04_gauge_mt8167_read(&a, 0x0b, 0x09, out));
    a.faulted = 0; m.outcome = 2;
    assert(a04_gauge_mt8167_read(&a, 0x0b, 0x09, out));
    assert(a.faulted && m.delays == 2000 && out[0] == 1);
    a.faulted = 0; m.outcome = 4;
    assert(a04_gauge_mt8167_read(&a, 0x0b, 0x09, out));
    assert(a.faulted);
    m.outcome = 0;
    assert(a04_gauge_mt8167_read(&a, 0x0b, 0x09, out));
    return 0;
}
