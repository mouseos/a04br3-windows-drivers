/* SPDX-License-Identifier: GPL-2.0-only */
#include "monitor.h"
static int stop(struct a04_monitor *m)
{ if(m && m->active) { m->active=0; return m->apply(m->context,A04_TEMP_HALT) ? -2 : -1; } return -1; }
int a04_monitor_start(struct a04_monitor *m,const struct a04_thermal_hw *h,
                      const struct a04_thermal_cal *cal,const struct a04_thermal_policy *policy,
                      void *ctx,int (*apply)(void *,enum a04_thermal_action),uint64_t now,uint64_t timeout)
{
 if(!m || !h || !cal || !policy || !apply || !timeout || !policy->max_age_ms ||
    policy->release_mc>=policy->limit_mc || policy->limit_mc>=policy->halt_mc) return -1;
 m->hw=h;m->calibration=*cal;m->policy=*policy;m->policy.limited=0;
 m->context=ctx;m->apply=apply;m->active=1;
 if(a04_thermal_hw_init(h) || a04_completion_start(&m->completion,now,timeout)) return stop(m);
 return 0;
}
int a04_monitor_poll(struct a04_monitor *m,uint64_t now,struct a04_thermal_sample *out)
{
 uint32_t raw[3]; struct a04_thermal_sample s; enum a04_thermal_action a; int r;
 if(!m || !m->active || !out) return -1;
 r=a04_completion_poll(&m->completion,m->hw,now,m->policy.max_age_ms,raw);
 if(r<0)return stop(m);
 if(r==0)return 0;
 if(a04_thermal_sample(&m->calibration,raw,now,1,&s))return stop(m);
 a=a04_thermal_decide(&m->policy,&s,now,1);
 if(a==A04_TEMP_HALT || m->apply(m->context,a))return stop(m);
 *out=s;return 1;
}
