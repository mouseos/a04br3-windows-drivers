/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "power_bridge.h"
static int fault(struct a04_wifi_power_bridge *b)
{ a04_wifi_power_fault(&b->plan);a04_wifi_fault(b->owner);return -1; }
static int valid(struct a04_wifi_power_bridge *b)
{ return b && b->owner && b->qualified && b->rail_clock_stable && b->release_reset &&
 b->rail_delay && b->reset_delay && !b->owner->poisoned && b->owner->active &&
 b->owner->generation==b->generation && b->qualified(b->context,b->generation)>0; }
int a04_wifi_power_start(struct a04_wifi_power_bridge *b,uint64_t now)
{
 if(!valid(b) || !now || b->plan.phase!=A04_WIFI_OFF)return -1;
 if(b->rail_clock_stable(b->context)<=0)return fault(b);
 return a04_wifi_power_begin(&b->plan,now,1,1,1,1);
}
int a04_wifi_power_step(struct a04_wifi_power_bridge *b,uint64_t now)
{
 int r;
 if(!b || !b->owner)return -1;
 if(!valid(b))return fault(b);
 if(b->rail_clock_stable(b->context)<=0)return fault(b);
 if(b->plan.phase==A04_WIFI_WAIT_RAIL){
  r=a04_wifi_power_rail_settled(&b->plan,now,b->rail_delay);
  if(r<0)return fault(b);
  if(r>0)return 1;
  /* Re-check after the wait and before the irreversible reset release. */
  if(!valid(b) || b->rail_clock_stable(b->context)<=0)return fault(b);
  if(b->release_reset(b->context)<=0)return fault(b);
  if(a04_wifi_power_reset_released(&b->plan,now,1))return fault(b);
  return 1;
 }
 if(b->plan.phase==A04_WIFI_WAIT_RESET){
  r=a04_wifi_power_ready(&b->plan,now,b->reset_delay);
  return r<0?fault(b):r;
 }
 return b->plan.phase==A04_WIFI_READY?0:-1;
}
