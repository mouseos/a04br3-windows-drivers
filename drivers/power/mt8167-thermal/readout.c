/* SPDX-License-Identifier: GPL-2.0-only */
#include "readout.h"
void a04_readout_invalidate(struct a04_thermal_readout_state *s)
{
 if(!s)return;
 s->valid=0;
 s->sample=(struct a04_thermal_sample){0};
}
int a04_readout_publish(struct a04_thermal_readout_state *s,const struct a04_thermal_sample *v)
{
 int32_t max;unsigned i;
 if(!s || !v)return -1;
 max=v->mc[0];
 for(i=0;i<3;i++) {
  if(v->mc[i]<-40000 || v->mc[i]>150000){a04_readout_invalidate(s);return -1;}
  if(v->mc[i]>max)max=v->mc[i];
 }
 if(max!=v->hottest_mc || (s->valid && v->time_ms<s->sample.time_ms)){a04_readout_invalidate(s);return -1;}
 s->sample=*v;
 if(++s->sequence==0)++s->sequence;
 s->valid=1;return 0;
}
int a04_readout_get(const struct a04_thermal_readout_state *s,uint64_t now,uint32_t max_age,struct a04_thermal_readout_v1 *out)
{
 struct a04_thermal_readout_v1 r;unsigned i;uint64_t age;
 if(!s || !out || s->valid!=1 || !max_age || now<s->sample.time_ms)return -1;
 age=now-s->sample.time_ms;
 if(age>max_age)return -1;
 r.version=1;r.sequence=s->sequence;
 for(i=0;i<3;i++)r.sensor_mc[i]=s->sample.mc[i];
 r.hottest_mc=s->sample.hottest_mc;r.age_ms=(uint32_t)age;
 *out=r;return 0;
}
