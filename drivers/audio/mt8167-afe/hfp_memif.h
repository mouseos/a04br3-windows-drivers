/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_HFP_MEMIF_H
#define MT8167_HFP_MEMIF_H
#include <stdint.h>
/* CT3 kernel-4.4 mt8167-afe-pcm.c: DL1/I05 mono -> MRG O02;
 * I02 -> O11 -> DAI, IRQ1 playback / IRQ2 capture, 8k S16_LE.
 * Caller owns the entire AFE lease, clocks and interrupt synchronization.
 * This is a register engine, not an independent DMA bus-drain certificate. */
struct mt8167_hfp_io {
 void *context;
 int (*read)(void *,unsigned,uint32_t *);
 int (*write)(void *,unsigned,uint32_t);
};
struct mt8167_hfp_ring {uint32_t base,bytes,generation;};
struct mt8167_hfp_memif {
 struct mt8167_hfp_io io;
 struct mt8167_hfp_ring client[2],fallback[2],current[2];
 uint32_t saved[11];unsigned prepared,running,active,exposed,fault,stopped;
};
#ifdef __cplusplus
extern "C" {
#endif
int mt8167_hfp_memif_prepare(struct mt8167_hfp_memif *,const struct mt8167_hfp_io *,
 const struct mt8167_hfp_ring fallback[2]);
int mt8167_hfp_memif_start(struct mt8167_hfp_memif *);
/* Disable only this memif. Owner must independently drain its current MDL
 * before bind/inactive resumes it; peer DMA/clocks remain enabled. */
int mt8167_hfp_memif_stop_direction(struct mt8167_hfp_memif *,unsigned);
int mt8167_hfp_memif_bind(struct mt8167_hfp_memif *,unsigned,const struct mt8167_hfp_ring *);
/* Switch to fallback only AFTER stop_direction + owner bus drain. */
int mt8167_hfp_memif_inactive(struct mt8167_hfp_memif *,unsigned);
int mt8167_hfp_memif_position(struct mt8167_hfp_memif *,unsigned,uint32_t,uint32_t *);
int mt8167_hfp_memif_irq(struct mt8167_hfp_memif *,uint32_t *);
int mt8167_hfp_memif_stop_registers(struct mt8167_hfp_memif *);
/* Only after actual external bus+IRQ/cache drain, not a caller Boolean. */
int mt8167_hfp_memif_restore(struct mt8167_hfp_memif *);
#ifdef __cplusplus
}
#endif
#endif
