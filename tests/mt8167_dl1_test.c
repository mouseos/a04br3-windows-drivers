#include <assert.h>
#include <stdint.h>
#include "dl1.h"
int main(void)
{
    struct mt8167_dl1_plan p = { 1, 2, 3, 4 };
    uint32_t pos = 123;
    assert(!mt8167_dl1_plan(0x1000, 1024, 256, 12345, &p));
    assert(p.base == 1 && p.end == 2 && p.rate_code == 3 && p.period_frames == 4);
    assert(mt8167_dl1_plan(0x1000, 1024, 256, 48000, &p));
    assert(p.base == 0x1000 && p.end == 0x13ff && p.rate_code == 10 && p.period_frames == 64);
    assert(mt8167_dl1_position(&p, 0x13fc, &pos) && pos == 1020);
    assert(!mt8167_dl1_position(&p, 0, &pos) && pos == 1020);
    assert(!mt8167_dl1_position(&p, 0x1400, &pos));
    assert(!mt8167_dl1_position(&p, 0x1001, &pos));
    assert(!mt8167_dl1_plan(0xfffffff0ULL, 1024, 256, 48000, &p));
    assert(!mt8167_dl1_plan(0x1000, 1024, 512, 48000, 0));
    assert(!mt8167_dl1_plan(0x1000, 512, 512, 48000, &p));
    assert(!mt8167_dl1_plan(0x1001, 1024, 256, 48000, &p));
    assert(!mt8167_dl1_plan(0x1000, 1024, 260, 48000, &p));
    assert(mt8167_dl1_plan(0xfffffc00ULL, 1024, 256, 44100, &p));
    assert(p.end == UINT32_MAX && p.rate_code == 9);
    return 0;
}
