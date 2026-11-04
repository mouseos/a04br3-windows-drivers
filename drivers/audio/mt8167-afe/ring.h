#ifndef MT8167_AFE_RING_H
#define MT8167_AFE_RING_H
#include <stdint.h>
struct mt8167_ring_span { uint32_t offset, length; };
/* Plan a PCM-frame-aligned transfer into a mapped ring; no memory access.
 * One transfer may cover at most one ring. Failure leaves spans unchanged.
 * Caller owns DMA memory and must synchronize CPU/device cache visibility. */
int mt8167_ring_spans(uint32_t size, uint32_t offset, uint32_t bytes,
                      struct mt8167_ring_span spans[2]);
#endif
