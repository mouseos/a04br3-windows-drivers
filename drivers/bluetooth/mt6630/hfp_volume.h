/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_HFP_VOLUME_H
#define A04_HFP_VOLUME_H
#include <stdint.h>
/* Unsigned differences cover the full signed LONG range without overflow.
 * No dB conversion, assumed headset range, FP or ARM divide helper. */
static inline uint32_t a04_hfp_volume_remainder(uint32_t delta,uint32_t unit)
{
 unsigned shifts=0;
 while(unit<=0x7fffffffu&&unit<=(delta>>1)){unit<<=1;shifts++;}
 for(;;){if(delta>=unit)delta-=unit;if(!shifts)break;unit>>=1;shifts--;}
 return delta;
}
static inline int a04_hfp_volume_aligned(int32_t minimum,int32_t maximum,uint32_t step,int32_t value)
{
 return minimum<=maximum&&step&&value>=minimum&&value<=maximum&&
  a04_hfp_volume_remainder((uint32_t)value-(uint32_t)minimum,step)==0;
}
static inline int a04_hfp_volume_select(int32_t minimum,int32_t maximum,uint32_t step,int32_t requested,int32_t *out)
{
 uint32_t delta;
 if(!out||minimum>maximum||!step)return 0;
 if(requested<minimum)requested=minimum;
 if(requested>maximum)requested=maximum;
 delta=(uint32_t)requested-(uint32_t)minimum;
 *out=(int32_t)((uint32_t)minimum+delta-a04_hfp_volume_remainder(delta,step));
 return 1;
}
#endif
