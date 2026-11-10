/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_CODEC_HEADPHONE_H
#define A04_MT8167_CODEC_HEADPHONE_H
#include <stdint.h>
/* Hardware facts: CT3 GPL7c478212, mt8167-codec.c DAPM HP path and
 * mt8167-codec-utils.c stereo DC latch/ramp. Not the MT6392 Class-D route.
 * No default, stored-device or user-supplied calibration provider exists here. */
struct a04_hp_calibration {
 uint32_t generation,measured_channels,cap_index,gain[2];
 int16_t vcm[2],dac[2],op[2]; /* selected gain, runtime-measured A/B/C result */
};
struct a04_hp_measurements {
 uint32_t generation,measured_channels,cap_index,gain[2];
 int32_t uv[2][3]; /* actual trimmed AUXADC mean A, B, C per selected gain */
};
/* Pure conversion only: does not assert or obtain AUXADC/thermal ownership. */
int a04_codec_headphone_calibration(const struct a04_hp_measurements *,struct a04_hp_calibration *);
struct a04_hp_io {
 void *context;
 /* Every callback executes under the existing exclusive parent audio lease.
  * qualified must verify live generation, common domain, Class-D mute,
  * resources/admission and absence of a speaker/other DL route. */
 int (*qualified)(void *,uint32_t generation);
 int (*read)(void *,unsigned bank,unsigned offset,uint32_t *);
 int (*write)(void *,unsigned bank,unsigned offset,uint32_t);
 int (*delay_us)(void *,unsigned);
 int (*verify_dma_irq_drained)(void *); /* independent proof, before stop */
};
struct a04_hp_route {
 struct a04_hp_calibration calibration;
 uint32_t saved[5],expected[5]; /* CON01, CON02, ABB0, L/R DC */
 int held,active,poisoned;
};
/* 0=complete, -1=reject before writes, -2=uncertain; retain owner on -2.
 * Caller acquires/releases common domain and DMA owner, never this module. */
int a04_codec_headphone_start_muted(struct a04_hp_route *,const struct a04_hp_io *,const struct a04_hp_calibration *);
int a04_codec_headphone_start(struct a04_hp_route *,const struct a04_hp_io *,const struct a04_hp_calibration *);
int a04_codec_headphone_mute_retained(struct a04_hp_route *,const struct a04_hp_io *);
int a04_codec_headphone_resume_retained(struct a04_hp_route *,const struct a04_hp_io *);
int a04_codec_headphone_stop(struct a04_hp_route *,const struct a04_hp_io *);
#endif
