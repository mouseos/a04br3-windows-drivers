/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_POLICY_H
#define A04_NATIVE_POLICY_H
#include <stdint.h>
/* Native POWER_MGMT_REQUEST defines four discrete levels, not a range.
 * MicrosoftDocs 8bcfbd3e:24 is an IHV mode beyond MAX, not MAX itself.
 * gen3 exposes only CAM0/MAX1/FAST2. Never invent a fourth FW profile. */
static inline int a04_native_power_profile(uint32_t level,unsigned *profile)
{
 if(!profile)return -1;
 switch(level){
 case 0:*profile=0;return 0;
 case 8:*profile=2;return 0;
 case 16:*profile=1;return 0;
 case 24:return -2; /* defined proprietary capability, not implemented */
 default:return -1; /* not a defined Native level */
 }
}
#define A04_UNREACHABLE_DEFAULT_MS 2000u
struct a04_peer_loss {uint32_t threshold_ms;uint64_t last_rx_us;unsigned suspected;};
/* IHV criterion: a real gen3 beacon-loss event AND no subsequent peer RX.
 * Silence alone is never evidence: firmware may consume/filter beacons. */
static inline void a04_peer_observed(struct a04_peer_loss *p,uint64_t time)
{p->last_rx_us=time;p->suspected=0;}
static inline int a04_peer_loss_expired(const struct a04_peer_loss *p,uint64_t time)
{return p->suspected&&time>=p->last_rx_us&&time-p->last_rx_us>=(uint64_t)p->threshold_ms*1000u;}
#endif
