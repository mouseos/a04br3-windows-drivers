/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_MT8167_CODEC_CALIBRATION_H
#define A04_MT8167_CODEC_CALIBRATION_H
#include <stdint.h>
/* Board Linux mt8167-codec-utils.c: A, B and C are measured in microvolts.
 * This only converts measurements; it neither obtains them nor proves that
 * the headphone calibration circuit represents the board's voice output. */
static int a04_codec_calculate_comp(int32_t before_uv,int32_t after_uv,
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
