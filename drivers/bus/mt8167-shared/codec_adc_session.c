/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "codec_adc_session.h"
enum a04_codec_adc_session_result a04_codec_adc_session(const struct a04_codec_adc_session_io *io,struct a04_codec_adc_samples *out)
{
 struct a04_codec_adc_samples tmp={0};
 if(!io || !out || !io->preflight || !io->clock_on || !io->capture || !io->quiesce || io->preflight(io->context))return A04_ADC_REJECTED;
 /* A failed clock operation or conversion may already have reached hardware.
  * Do not attempt cleanup or release unless a device-specific recovery exists. */
 if(io->clock_on(io->context) || io->capture(io->context,&tmp) || io->quiesce(io->context))return A04_ADC_UNCERTAIN;
 *out=tmp;return A04_ADC_COMPLETE;
}
