#ifndef MT8167_DL1_ENGINE_H
#define MT8167_DL1_ENGINE_H
#include "dl1.h"
/* Transport must own AFE exclusively, serialize every call including ISR, use
 * ordered MMIO, and keep DMA mapped/coherent until confirmed stopped. This
 * component cannot establish these prerequisites. It is NOT connected to the
 * adapter. A failed or ambiguous transaction poisons the engine. */
struct mt8167_dl1_io {
    void *context;
    int (*read32)(void *, uint32_t, uint32_t *);
    int (*write32)(void *, uint32_t, uint32_t);
};
enum mt8167_dl1_state { MT8167_DL1_IDLE, MT8167_DL1_RUNNING,
                        MT8167_DL1_STOPPED, MT8167_DL1_POISONED };
struct mt8167_dl1_engine { struct mt8167_dl1_io io; enum mt8167_dl1_state state; };
/* prepare/start assumes no DMA active, a muted initialized codec and a buffer
 * already made visible to the device by its DMA owner. No clocks are changed. */
#ifdef __cplusplus
extern "C" {
#endif
int mt8167_dl1_start(struct mt8167_dl1_engine *, const struct mt8167_dl1_plan *);
/* On failure retain DMA mapping and contents; hardware quiescence is unknown. */
int mt8167_dl1_stop(struct mt8167_dl1_engine *);
/* Returns 1 for own interrupt, 0 for other IRQ, -1 on error. Caller schedules
 * notification outside its interrupt lock and handles DPC lifetime. */
int mt8167_dl1_interrupt(struct mt8167_dl1_engine *, int *notify);
#ifdef __cplusplus
}
#endif
#endif
