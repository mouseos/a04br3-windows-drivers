#ifndef MT8167_AUDIO_RESOURCE_CHECK_H
#define MT8167_AUDIO_RESOURCE_CHECK_H
#include <stdint.h>
/* Descriptor shape only; does not establish ownership, DMA or IRQ routing. */
static int mt8167_audio_resources_match(uint32_t total, uint32_t mem_count,
    uint32_t irq_count, uint64_t a0, uint32_t n0, uint64_t a1, uint32_t n1,
    int irq_level)
{
    int first = a0 == 0x11140000u && n0 == 0x1000u && a1 == 0x11141000u && n1 == 0x9000u;
    int second = a1 == 0x11140000u && n1 == 0x1000u && a0 == 0x11141000u && n0 == 0x9000u;
    return total == 3 && mem_count == 2 && irq_count == 1 && irq_level && (first || second);
}
#endif
