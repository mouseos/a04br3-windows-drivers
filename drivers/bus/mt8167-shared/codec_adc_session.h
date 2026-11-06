/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_CODEC_ADC_SESSION_H
#define A04_CODEC_ADC_SESSION_H
#include "codec_adc_probe.h"
/* Serialized, one-shot lease. preflight establishes independently verified
 * route, mute, exclusive ownership and resource/power lifetime without MMIO
 * writes. A clock or transaction error can leave hardware changed. */
struct a04_codec_adc_session_io {
 void *context;
 int (*preflight)(void *);
 int (*clock_on)(void *);
 int (*capture)(void *,struct a04_codec_adc_samples *);
 int (*quiesce)(void *);
};
enum a04_codec_adc_session_result { A04_ADC_REJECTED, A04_ADC_COMPLETE, A04_ADC_UNCERTAIN };
/* REJECTED precedes writes. UNCERTAIN requires retained ownership and recovery.
 * COMPLETE means observation and quiesce only, not calibrated voltage or
 * proof of delivery. Caller releases the shared lease after COMPLETE. */
enum a04_codec_adc_session_result a04_codec_adc_session(const struct a04_codec_adc_session_io *,struct a04_codec_adc_samples *);
#endif
