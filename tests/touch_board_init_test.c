/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "board_init.h"
struct step { int kind; unsigned bank, offset, mask, value; };
static const struct step expected[] = {
 {0,0,0x3b0,7u<<9,1u<<9}, {0,0,0x3b0,7u<<12,1u<<12},
 {0,0,0x310,7u<<9,0}, {1,0,0x08,0,1u<<8},
 {1,0,0x904,0,(1u<<3)|(1u<<15)}, {1,1,0x84,0,1u<<3},
 {0,2,0x80,1u<<1,0}, {2,0,0x50,0,1}, {2,0,0x40,0,3},
 {2,0,0x54,0,0}, {2,0,0x70,0,4}, {2,0,0x20,0,0x73f},
 {2,0,0x48,0,0}, {2,0,0x28,0,0x8001}
};
struct context { unsigned count, fail_at; };
static int step(struct context *c, int kind, unsigned bank, unsigned off,
                unsigned mask, unsigned value)
{
    const struct step *s;
    assert(c->count < sizeof(expected)/sizeof(expected[0]));
    s = &expected[c->count++];
    assert(s->kind == kind && s->bank == bank && s->offset == off &&
           s->mask == mask && s->value == value);
    return c->count == c->fail_at ? -1 : 0;
}
static int update(void *p, enum a04_touch_register_bank b, unsigned o, uint32_t m, uint32_t v)
{ return step(p, 0, b, o, m, v); }
static int write32(void *p, enum a04_touch_register_bank b, unsigned o, uint32_t v)
{ return step(p, 1, b, o, 0, v); }
static int write16(void *p, unsigned o, uint16_t v)
{ return step(p, 2, 0, o, 0, v); }
int main(void)
{
    struct context c = {0, 0};
    struct a04_touch_board_io io = {&c, update, write32, write16};
    unsigned n;
    assert(a04_touch_board_init(&io) == 0 && c.count == sizeof(expected)/sizeof(expected[0]));
    for (n = 1; n <= sizeof(expected)/sizeof(expected[0]); ++n) {
        c.count = 0; c.fail_at = n;
        assert(a04_touch_board_init(&io) != 0 && c.count == n);
    }
    c.count = 7; c.fail_at = 0;
    assert(a04_touch_i2c_reset(&io) == 0 && c.count == sizeof(expected)/sizeof(expected[0]));
    for (n = 8; n <= sizeof(expected)/sizeof(expected[0]); ++n) {
        c.count = 7; c.fail_at = n;
        assert(a04_touch_i2c_reset(&io) != 0 && c.count == n);
    }
    assert(a04_touch_i2c_reset(NULL) != 0);
    assert(a04_touch_board_init(NULL) != 0);
    puts("touch board setup tests passed");
    return 0;
}
