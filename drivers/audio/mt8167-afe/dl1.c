#include "dl1.h"

int mt8167_dl1_plan(uint64_t address, uint32_t bytes, uint32_t period,
                    uint32_t rate, struct mt8167_dl1_plan *out)
{
    struct mt8167_dl1_plan p;
    uint32_t code;
    if (!out || !address || address > UINT32_MAX || bytes > 1048576 ||
        period < 256 || period > 524288 || bytes < period ||
        bytes / period < 2 || bytes % period || (address & 3) ||
        (bytes & 3) || (period & 3) || address + bytes - 1 > UINT32_MAX ||
        period / 4 > 0x3ffff) return 0;
    switch (rate) {
    case 8000: code = 0; break;
    case 11025: code = 1; break;
    case 12000: code = 2; break;
    case 16000: code = 4; break;
    case 22050: code = 5; break;
    case 24000: code = 6; break;
    case 32000: code = 8; break;
    case 44100: code = 9; break;
    case 48000: code = 10; break;
    case 96000: code = 12; break;
    default: return 0;
    }
    p.base = (uint32_t)address;
    p.end = (uint32_t)(address + bytes - 1);
    p.rate_code = code;
    p.period_frames = period / 4;
    *out = p;
    return 1;
}
int mt8167_dl1_position(const struct mt8167_dl1_plan *plan,
                        uint32_t current, uint32_t *offset)
{
    if (!plan || !offset || !plan->base || plan->end < plan->base ||
        current < plan->base || current > plan->end ||
        ((current - plan->base) & 3)) return 0;
    *offset = current - plan->base;
    return 1;
}
