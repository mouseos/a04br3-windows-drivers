/* MT8167 AFE DL1 parameter validation; no hardware access. */
#ifndef A04_MT8167_DL1_H
#define A04_MT8167_DL1_H
#include <stdint.h>
struct mt8167_dl1_plan {
    uint32_t base, end, rate_code, period_frames;
};
/* Stereo signed 16-bit PCM only. DMA mapping and ownership are caller duties.
 * On failure, *out is unchanged. The returned end address is inclusive. */
int mt8167_dl1_plan(uint64_t dma_address, uint32_t buffer_bytes,
                    uint32_t period_bytes, uint32_t sample_rate,
                    struct mt8167_dl1_plan *out);
/* Reject zero/uninitialized and out-of-range hardware pointers. */
int mt8167_dl1_position(const struct mt8167_dl1_plan *plan,
                        uint32_t current, uint32_t *byte_offset);
#endif
