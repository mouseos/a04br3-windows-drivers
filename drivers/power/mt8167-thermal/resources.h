/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef A04_THERMAL_RESOURCES_H
#define A04_THERMAL_RESOURCES_H
#include <stdint.h>
/* Caller provides paired raw/translated descriptors from the same PnP list
 * index. This validates assignment only, never firmware handoff or D0. */
struct a04_thermal_resource { uint64_t raw,translated;uint32_t raw_size,size;int raw_memory,memory,raw_exclusive,exclusive; };
static inline int a04_thermal_resources_validate(const struct a04_thermal_resource *r,unsigned count)
{
 static const uint64_t bases[]={0x1100d000u,0x11003000u,0x10018000u,0x10009000u};
 unsigned i,j,mask=0;
 if(!r || count!=4)return -1;
 for(i=0;i<count;i++){
  if(!r[i].raw_memory || !r[i].memory || !r[i].raw_exclusive || !r[i].exclusive ||
     r[i].raw_size!=0x1000 || r[i].size!=0x1000 || r[i].raw!=r[i].translated)return -1;
  for(j=0;j<4;j++)if(r[i].translated==bases[j])break;
  if(j==4 || (mask&(1u<<j)))return -1;
  mask|=1u<<j;
 }
 return mask==15?0:-1;
}
#endif
