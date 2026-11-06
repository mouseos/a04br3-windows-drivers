/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_CODEC_ADC_PROBE_H
#define A04_CODEC_ADC_PROBE_H
#include <stdint.h>
/* Diagnostic only. qualified must verify an exclusive AUXADC lease, active
 * clock, stable analog route and downstream mute. Caller serializes it with
 * all ADC/thermal users and retains resources after any poisoned operation.
 * This does not configure an analog route or prove AU_LOL calibration. */
struct a04_codec_adc_io {
 void *context;
 int (*qualified)(void *);
 int (*read16)(void *,uint32_t,uint16_t *);
 int (*write16)(void *,uint32_t,uint16_t);
 int (*delay_us)(void *,unsigned);
};
struct a04_codec_adc_probe { int poisoned; };
struct a04_codec_adc_samples { uint16_t raw[22]; int32_t average_uv; };
/* Channels 7 and 8 are headphone observations only. Outputs are unchanged on
 * failure. No implied DMA, calibration, route, or drain proof. */
int a04_codec_adc_capture(struct a04_codec_adc_probe *,const struct a04_codec_adc_io *,unsigned,struct a04_codec_adc_samples *);
#endif
