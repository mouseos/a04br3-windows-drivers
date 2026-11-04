/* SPDX-License-Identifier: GPL-2.0-only */
#include "freshness.h"
int a04_freshness_update(struct a04_freshness *s,const uint32_t raw[3],uint64_t now,uint64_t max_age)
{
 unsigned i; uint16_t value;
 if(!s || !raw || !max_age || (s->seen && now<s->last_ms)) return -1;
 for(i=0;i<3;i++) if(!(raw[i]&0xfffu)) { s->seen=0; s->changed=0; return -1; }
 for(i=0;i<3;i++) {
  value=(uint16_t)(raw[i]&0xfffu);
  if(!(s->seen&(1u<<i))) { s->last[i]=value; s->changed_ms[i]=now; s->seen|=1u<<i; }
  else if(value!=s->last[i]) { s->last[i]=value; s->changed_ms[i]=now; s->changed|=1u<<i; }
 }
 s->last_ms=now;
 if(s->changed!=7u) return -1;
 for(i=0;i<3;i++) if(now-s->changed_ms[i]>max_age) return -1;
 return 0;
}
