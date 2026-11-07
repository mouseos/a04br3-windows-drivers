/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_OS_RSN_POLICY_H
#define A04_OS_RSN_POLICY_H
#include "wmt_bridge.h"
/* Negotiate RSNA-PSK/CCMP128 only. MFPC is optional and may be declined;
 * MFPR, non-CCMP group suites, absent PSK and malformed lists are rejected. */
static inline int a04_os_rsn_policy(const uint8_t *p,unsigned n)
{
 static const uint8_t aes[4]={0,15,172,4},psk[4]={0,15,172,2};unsigned off=8,count,found=0,caps;
 if(!p||n<20||p[0]!=48||p[1]+2u!=n||a04_wmt_le16(p+2)!=1||!a04_wlan_equal(p+4,aes,4))return -1;
 count=a04_wmt_le16(p+off);off+=2;if(!count||count>(n-off)/4)return -1;
 for(unsigned i=0;i<count;i++,off+=4)found|=a04_wlan_equal(p+off,aes,4);
 if(!found||n-off<2)return -1;count=a04_wmt_le16(p+off);off+=2;found=0;if(!count||count>(n-off)/4)return -1;
 for(unsigned i=0;i<count;i++,off+=4)found|=a04_wlan_equal(p+off,psk,4);if(!found)return -1;
 if(off==n)return 0;if(n-off<2)return -1;caps=a04_wmt_le16(p+off);off+=2;if(caps&0x40)return -1;
 if(off==n)return 0;if(n-off<2)return -1;count=a04_wmt_le16(p+off);off+=2;if(count>(n-off)/16)return -1;off+=16*count;
 if(off==n)return 0;return (n-off==4&&(caps&0x80)&&p[off]==0&&p[off+1]==15&&p[off+2]==172&&p[off+3]==6)?0:-1;
}
/* Linux gen3 scanSearchBssDescByPolicy checks security before selecting a
 * same-SSID candidate. Cache order must not override the OS cipher choice. */
static inline int a04_os_candidate_policy(const struct a04_wlan_ap *ap,unsigned active,unsigned secured)
{
 if(!ap||secured>1)return A04_WLAN_ARGUMENT;
 if(!ap->channel||ap->channel>14||!(active&(1u<<ap->channel)))return A04_WLAN_UNSUPPORTED;
 if(!!(ap->capability&16u)!=!!secured)return A04_WLAN_SECURITY;
 if(secured&&a04_os_rsn_policy(ap->rsn,ap->rsn_length))return A04_WLAN_SECURITY;
 return 0;
}
#endif
