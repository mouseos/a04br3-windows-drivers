/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "mma8652.h"
struct fake { unsigned calls; int fail; uint8_t id, data[7]; };
static int read_regs(void *ctx, uint8_t addr, uint8_t reg, uint8_t *out, unsigned len)
{
    struct fake *f = ctx; ++f->calls; assert(addr == 0x1d);
    if (f->fail) return -1;
    if (reg == 0x0d) { assert(len == 1); *out = f->id; }
    else { assert(reg == 0 && len == 7); memcpy(out, f->data, len); }
    return 0;
}
int main(void)
{
    struct fake f = {0}; struct a04_mma8652_bus bus = {&f, read_regs, 0};
    struct a04_mma8652_sample s = {11, 22, 33};
    assert(a04_mma8652_probe(0) == A04_MMA_INVALID);
    f.id = 0x4a; assert(a04_mma8652_probe(&bus) == A04_MMA_OK);
    f.id = 0x2a; assert(a04_mma8652_probe(&bus) == A04_MMA_ID);
    f.fail = 1; assert(a04_mma8652_probe(&bus) == A04_MMA_IO);
    assert(a04_mma8652_read(&bus, &s) == A04_MMA_IO);
    f.fail = 0; assert(a04_mma8652_read(&bus, &s) == A04_MMA_NOT_READY);
    assert(s.x == 11 && s.y == 22 && s.z == 33);
    /* Sensor X=-2048 Y=2047 Z=-1024, 12-bit left aligned. */
    { const uint8_t frame[] = {8,0x80,0,0x7f,0xf0,0xc0,0}; memcpy(f.data, frame, 7); }
    assert(a04_mma8652_read(&bus, &s) == A04_MMA_OK);
    assert(s.x == -2047 && s.y == 2048 && s.z == 1024);
    assert(a04_mma8652_read(&bus, 0) == A04_MMA_INVALID);
    puts("MMA8652 tests passed"); return 0;
}
