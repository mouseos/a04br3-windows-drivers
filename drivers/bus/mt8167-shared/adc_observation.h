/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_ADC_OBSERVATION_H
#define A04_ADC_OBSERVATION_H
#include <stdint.h>
/* Must match A04BR3Pkg/Include/A04BR3AdcObservation.h. Only early DXE
 * clock status; never evidence of EBS, secure release or exclusivity. */
struct a04_adc_observation { uint32_t version,bytes,phase,clock_first,clock_second,secure_release; };
static inline int a04_adc_observation_valid(const struct a04_adc_observation *v,unsigned size,unsigned attrs)
{
 return v && size==sizeof(*v) && attrs==6u && v->version==1 && v->bytes==sizeof(*v) &&
        v->phase==1 && v->clock_first!=0xffffffffu && v->clock_first==v->clock_second && v->secure_release==0;
}
#endif
