#include "pmic_read.h"
#include <assert.h>
#include <stdio.h>

struct mock { unsigned stage, enabled, stuck, writes, delays; };
static uint32_t rd(void *p, unsigned reg)
{
    struct mock *m = p;
    if (reg == 0x9c) return m->enabled;
    assert(reg == 0xa4);
    if (m->stuck == m->stage + 1) return 3u << 16;
    return m->stage == 1 ? (6u << 16) | 0xa55au : 0;
}
static void wr(void *p, unsigned reg, uint32_t val)
{
    struct mock *m = p;
    if (m->stage == 0) { assert(reg == 0xa0 && val == 0x00a10000u); m->stage = 1; }
    else { assert(m->stage == 1 && reg == 0xa8 && val == 1); m->stage = 2; }
    ++m->writes;
}
static void delay(void *p) { ++((struct mock *)p)->delays; }
int main(void)
{
    struct mock m = {0,1,0,0,0};
    struct a04_pmic_read b = {&m, rd, wr, delay, 3, 0};
    uint16_t value = 0;
    unsigned i;
    assert(a04_pmic_chrstatus(&b, &value) == 0 && value == 0xa55a && m.writes == 2);
    for (i = 1; i <= 3; ++i) {
        m = (struct mock){0,1,i,0,0}; b.poisoned = 0; value = 0xbeef;
        assert(a04_pmic_chrstatus(&b, &value) != 0 && value == 0xbeef && b.poisoned);
        { unsigned writes = m.writes; assert(a04_pmic_chrstatus(&b, &value) != 0 && m.writes == writes); }
    }
    m = (struct mock){0,0,0,0,0}; b.poisoned = 0;
    assert(a04_pmic_chrstatus(&b, &value) != 0 && !m.writes);
    puts("PMIC read sequencing and failure handling verified");
    return 0;
}
