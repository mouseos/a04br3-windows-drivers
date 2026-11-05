#include "dl1_engine.h"
#define DAC0 0x10u
#define DAC1 0x14u
#define DL1_BASE 0x40u
#define DL1_END 0x48u
#define IRQ_CON 0x3a0u
#define IRQ_STATUS 0x3a4u
#define IRQ_CLEAR 0x3a8u
#define IRQ_COUNT1 0x3acu
#define PBUF 0x3d8u
static int update(struct mt8167_dl1_engine *e, uint32_t off, uint32_t mask, uint32_t value)
{
    uint32_t old;
    if (e->io.read32(e->io.context, off, &old)) return -1;
    return e->io.write32(e->io.context, off, (old & ~mask) | (value & mask));
}
static int bad(struct mt8167_dl1_engine *e) { e->state = MT8167_DL1_POISONED; return -1; }
int mt8167_dl1_prepare_masked_irq(struct mt8167_dl1_engine *e)
{
    uint32_t dac,irq,status;
    if (!e || !e->io.read32 || !e->io.write32 || e->state!=MT8167_DL1_IDLE) return -1;
    /* Never mask or acknowledge a source that may belong to an active DL1. */
    if (e->io.read32(e->io.context,DAC0,&dac) || (dac&2u)) return bad(e);
    if (update(e,IRQ_CON,1u,0) || e->io.write32(e->io.context,IRQ_CLEAR,1u) ||
        e->io.read32(e->io.context,IRQ_CON,&irq) || (irq&1u) ||
        e->io.read32(e->io.context,IRQ_STATUS,&status) || (status&1u)) return bad(e);
    return 0;
}
static int plan_valid(const struct mt8167_dl1_plan *p)
{
    static const uint32_t rates[16] = {8000,11025,12000,0,16000,22050,24000,0,32000,44100,48000,0,96000,0,0,0};
    struct mt8167_dl1_plan validated;
    uint64_t length;
    if (!p || p->rate_code >= 16 || !rates[p->rate_code] || p->end < p->base ||
        p->period_frames > 0x3ffffu) return 0;
    length = (uint64_t)p->end - p->base + 1;
    if (length > 1048576u || !mt8167_dl1_plan(p->base, (uint32_t)length,
        p->period_frames * 4u, rates[p->rate_code], &validated)) return 0;
    return validated.base == p->base && validated.end == p->end &&
           validated.rate_code == p->rate_code && validated.period_frames == p->period_frames;
}
int mt8167_dl1_start(struct mt8167_dl1_engine *e, const struct mt8167_dl1_plan *p)
{
    uint32_t dac, irq;
    if (!e || !p || !e->io.read32 || !e->io.write32 || e->state != MT8167_DL1_IDLE ||
        !plan_valid(p))
        return -1;
    if (e->io.read32(e->io.context, DAC0, &dac) ||
        e->io.read32(e->io.context, IRQ_CON, &irq)) return bad(e);
    if ((dac & 2u) || (irq & 1u)) return bad(e);
    if (e->io.write32(e->io.context, DL1_BASE, p->base) ||
        e->io.write32(e->io.context, DL1_END, p->end) ||
        update(e, DAC1, (1u << 21) | 15u, p->rate_code) ||
        update(e, PBUF, 3u << 16, 0) ||
        update(e, IRQ_COUNT1, 0x3ffffu, p->period_frames) ||
        update(e, IRQ_CON, 15u << 4, p->rate_code << 4) ||
        e->io.write32(e->io.context, IRQ_CLEAR, 1u) ||
        update(e, DAC0, 2u, 2u) || update(e, IRQ_CON, 1u, 1u)) return bad(e);
    e->state = MT8167_DL1_RUNNING;
    return 0;
}
int mt8167_dl1_stop(struct mt8167_dl1_engine *e)
{
    uint32_t dac, irq;
    if (!e || e->state != MT8167_DL1_RUNNING) return -1;
    if (update(e, DAC0, 2u, 0) || update(e, IRQ_CON, 1u, 0) ||
        e->io.write32(e->io.context, IRQ_CLEAR, 1u) ||
        e->io.read32(e->io.context, DAC0, &dac) ||
        e->io.read32(e->io.context, IRQ_CON, &irq) || (dac & 2u) || (irq & 1u)) return bad(e);
    /* Readback of enable bits alone is NOT proof DMA has drained. */
    e->state = MT8167_DL1_STOPPED;
    return 0;
}
int mt8167_dl1_interrupt(struct mt8167_dl1_engine *e, int *notify)
{
    uint32_t status, dac;
    if (!e || !notify || e->state != MT8167_DL1_RUNNING) return -1;
    *notify = 0;
    if (e->io.read32(e->io.context, IRQ_STATUS, &status)) return bad(e);
    if (!(status & 1u)) return 0;
    if (e->io.read32(e->io.context, DAC0, &dac) ||
        e->io.write32(e->io.context, IRQ_CLEAR, 1u)) return bad(e);
    if (!(dac & 2u)) return bad(e);
    *notify = 1;
    return 1;
}
