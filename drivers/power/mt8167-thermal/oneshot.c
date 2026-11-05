/* SPDX-License-Identifier: GPL-2.0-only */
#include "oneshot.h"
enum a04_oneshot_result a04_thermal_oneshot(const struct a04_oneshot_io *io,const struct a04_thermal_cal *cal)
{
 struct a04_completion c;struct a04_thermal_sample sample;uint32_t raw[3];
 uint64_t now,start;unsigned n;int r;enum a04_oneshot_result result=A04_ONESHOT_INIT_FAILED;
 if(!io || !io->hw || !cal || !io->now_ms || !io->wait_ms || !io->set_ready || !io->report ||
    !io->hw->qualified || !io->hw->qualified(io->hw->ctx))return A04_ONESHOT_INVALID;
 /* A failed initialization does not reveal whether clocks were enabled.
  * Do not attempt further MMIO; the caller must retain the lease. */
 if(a04_thermal_hw_init(io->hw))return A04_ONESHOT_SHUTDOWN_FAILED;
 if(io->set_ready(io->context,1))goto shutdown;
 start=io->now_ms(io->context);
 if(a04_completion_start(&c,start,2000))goto shutdown;
 for(n=0;n<200;n++) {
  now=io->now_ms(io->context);
  if(now<start || now-start>2000) {result=A04_ONESHOT_TIMEOUT;goto shutdown;}
  r=a04_completion_poll(&c,io->hw,now,1000,raw);
  if(r<0) {result=A04_ONESHOT_SAMPLE_FAILED;goto shutdown;}
  if(r==1) {
   if(a04_thermal_sample(cal,raw,now,1,&sample))result=A04_ONESHOT_SAMPLE_FAILED;
   else result=A04_ONESHOT_OK;
   goto shutdown;
  }
  if(io->wait_ms(io->context,10)) {result=A04_ONESHOT_SAMPLE_FAILED;goto shutdown;}
 }
 result=A04_ONESHOT_TIMEOUT;
shutdown:
 if(a04_thermal_hw_quiesce(io->hw))return A04_ONESHOT_SHUTDOWN_FAILED;
 if(io->set_ready(io->context,0))return A04_ONESHOT_SHUTDOWN_FAILED;
 if(result==A04_ONESHOT_OK && io->report(io->context,&sample))return A04_ONESHOT_REPORT_FAILED;
 return result;
}
