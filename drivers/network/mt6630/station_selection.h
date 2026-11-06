/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_STATION_SELECTION_H
#define A04_STATION_SELECTION_H
#include "rsn_policy.h"
/* Apply the same eligibility predicate before Native association reporting
 * and before the firmware station transaction. An incompatible first BSS
 * must not hide another compatible BSS with the same SSID. Board gen3
 * mgmt/scan.c scanSanityCheckBssDesc likewise filters channel/security per BSS.
 * Keep the existing ERP20, active-channel and RSNA-PSK/CCMP subset. */
static inline int a04_station_candidate_matches(const struct a04_wlan_local *local,
 const struct a04_wlan_ap *ap,const uint8_t *ssid,unsigned length,unsigned secured)
{
 if(!local||!ap||!ssid||!length||length>32||secured>1)return 0;
 if(ap->ssid_length!=length||!a04_wlan_equal(ap->ssid,ssid,length))return 0;
 if(!ap->channel||ap->channel>13||!(local->active_channels&local->permitted_channels&(1u<<ap->channel)))return 0;
 if(!!(ap->capability&16u)!=!!secured)return 0;
 return !secured||!a04_os_rsn_policy(ap->rsn,ap->rsn_length);
}
#endif
