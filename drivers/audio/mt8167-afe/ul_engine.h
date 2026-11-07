/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_UL_ENGINE_H
#define MT8167_UL_ENGINE_H
#include <stdint.h>
/* Fixed INT ADDA -> I03/I04 -> O09/O10 -> VUL, stereo S16/48k.
 * Exclusive AFE owner serializes all calls with its ISR. Parent must hold
 * actual ADC/codec/clock ownership; this engine never fabricates that lease.
 * DMA allocation/mapping stays pinned after any address publication. */
struct mt8167_ul_io {
 void *context;int (*read)(void *,uint32_t,uint32_t *);
 int (*write)(void *,uint32_t,uint32_t);
};
struct mt8167_ul_engine {
 struct mt8167_ul_io io;
 uint32_t base,bytes,period_frames;
 unsigned prepared,running,exposed,fault;
};
#ifdef __cplusplus
extern "C" {
#endif
int mt8167_ul_prepare(struct mt8167_ul_engine *,uint64_t,uint32_t,uint32_t);
int mt8167_ul_start(struct mt8167_ul_engine *);
int mt8167_ul_stop(struct mt8167_ul_engine *);
int mt8167_ul_position(struct mt8167_ul_engine *,uint32_t *);
int mt8167_ul_interrupt(struct mt8167_ul_engine *,uint32_t *);
#ifdef __cplusplus
}
#endif
#endif
