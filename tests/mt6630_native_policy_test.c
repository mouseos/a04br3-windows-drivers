/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "native_policy.h"
int main(void)
{
 unsigned profile=99;
 assert(!a04_native_power_profile(0,&profile)&&profile==0);
 assert(!a04_native_power_profile(8,&profile)&&profile==2);
 assert(!a04_native_power_profile(16,&profile)&&profile==1);
 assert(a04_native_power_profile(24,&profile)==-2&&profile==1);
 for(unsigned i=1;i<24;i++)if(i!=8&&i!=16)assert(a04_native_power_profile(i,&profile)==-1&&profile==1);
 assert(a04_native_power_profile(UINT32_MAX,&profile)==-1&&profile==1);
 struct a04_peer_loss loss={A04_UNREACHABLE_DEFAULT_MS,100,0};
 assert(!a04_peer_loss_expired(&loss,UINT64_MAX)); /* silence is not evidence */
 loss.suspected=1;
 assert(!a04_peer_loss_expired(&loss,99)); /* monotonic guard */
 assert(!a04_peer_loss_expired(&loss,2000099)&&a04_peer_loss_expired(&loss,2000100));
 a04_peer_observed(&loss,2000100);
 assert(!a04_peer_loss_expired(&loss,9000000)); /* recovered RX cancels evidence */
 loss.suspected=1;loss.threshold_ms=7000;
 assert(!a04_peer_loss_expired(&loss,9000099)&&a04_peer_loss_expired(&loss,9000100));
 loss.threshold_ms=0;assert(a04_peer_loss_expired(&loss,2000100));
 loss.threshold_ms=UINT32_MAX;
 assert(!a04_peer_loss_expired(&loss,loss.last_rx_us+(uint64_t)UINT32_MAX*1000-1));
 assert(a04_peer_loss_expired(&loss,loss.last_rx_us+(uint64_t)UINT32_MAX*1000));
 puts("Native discrete power levels and evidence-gated millisecond peer-loss policy: OK");
 return 0;
}
