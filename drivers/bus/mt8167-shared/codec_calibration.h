/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_CODEC_CALIBRATION_H
#define A04_MT8167_CODEC_CALIBRATION_H
#include <stdint.h>
/* Linux IMM_auxadc_GetOneChannelValue_Cali uses a nominal 1.5 V
 * reference and 12-bit raw samples; it does not calibrate voltage. */
static inline int a04_codec_nominal_hp_uv(uint32_t raw, int32_t *result)
{
 if(!result || raw>4095)return -1;
 *result=(int32_t)(((uint64_t)raw*1500000u)/4096u);return 0;
}
/* Average 22 already-converted microvolt samples, dropping one minimum and
 * maximum. This is an offline calculation, not evidence of ADC ownership,
 * calibration, or applicability of the headphone path to AU_LOL. */
static inline int a04_codec_average_hp_uv(const int32_t *samples, unsigned count,
                                   int32_t *result)
{
 int64_t sum=0; int32_t min,max; unsigned i;
 if(!samples || !result || count!=22)return -1;
 min=max=samples[0];
 for(i=0;i<count;i++) {
  if(samples[i]<0 || samples[i]>1500000)return -1;
  if(samples[i]<min)min=samples[i];
  if(samples[i]>max)max=samples[i];
  sum+=samples[i];
 }
 *result=(int32_t)((sum-min-max)/20);return 0;
}
/* Board Linux mt8167-codec-utils.c: A, B and C are measured in microvolts.
 * This only converts measurements; it neither obtains them nor proves that
 * the headphone calibration circuit represents the board's voice output. */
static inline int a04_codec_calculate_comp(int32_t before_uv,int32_t after_uv,
                                    unsigned pga,int16_t *result)
{
 static const int32_t coeff[8]={4853,3855,3062,2432,1932,1534,1219,968};
 int64_t offset,code;
 if(!result || pga>=8)return -1;
 offset=((int64_t)before_uv-after_uv)/100;
 code=-(offset*coeff[pga]/1000);
 if(code < -32768 || code > 32767)return -1;
 *result=(int16_t)code;return 0;
}
#endif
