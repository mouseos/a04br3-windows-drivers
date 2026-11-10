#ifndef MT8167_DL1_ENGINE_H
#define MT8167_DL1_ENGINE_H
#include "dl1.h"
/* Transport must own AFE exclusively, serialize every call including ISR, use
 * ordered MMIO, and keep DMA mapped/coherent until confirmed stopped. This
 * component cannot establish these prerequisites. The Engine PCM backend
 * supplies them. A failed or ambiguous transaction poisons the engine. */
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
/* Exclusive AFE owner only, before connecting ISR: with clock held and all
 * other sources excluded, verify DL1 stopped, mask IRQ1, clear its latch and
 * verify mask/status. Failure poisons the engine and requires retaining the
 * ownership and mapping; this alone does not prove DMA bus drain. */
int mt8167_dl1_prepare_masked_irq(struct mt8167_dl1_engine *);
int mt8167_dl1_start(struct mt8167_dl1_engine *, const struct mt8167_dl1_plan *);
/* Only after verified STOP, with the same retained ring. reset=0 is the
 * Linux RESUME trigger; reset=1 repeats BASE/END setup for STOP/reopen.
 * Neither permits freeing or replacing the allocation. */
int mt8167_dl1_restart(struct mt8167_dl1_engine *,const struct mt8167_dl1_plan *,int reset);
/* On failure retain DMA mapping and contents; hardware quiescence is unknown. */
int mt8167_dl1_stop(struct mt8167_dl1_engine *);
/* Fault containment under the same interrupt synchronization lock, with the
 * parent's exclusive powered AFE lease held. Mask/clear/read back IRQ1 only;
 * preserve every other source and never report DMA stopped or bus drained.
 * Poison remains sticky even when the mask succeeds. */
int mt8167_dl1_mask_fault(struct mt8167_dl1_engine *);
/* Returns 1 for own interrupt, 0 for other IRQ, -1 on error. Caller schedules
 * notification outside its interrupt lock and handles DPC lifetime.
 * On error, *notify=-1 means IRQ1 was observed pending (claim the interrupt),
 * while 0 means ownership could not be established. Neither is a period. */
int mt8167_dl1_interrupt(struct mt8167_dl1_engine *, int *notify);
#ifdef __cplusplus
}
#endif
#endif
