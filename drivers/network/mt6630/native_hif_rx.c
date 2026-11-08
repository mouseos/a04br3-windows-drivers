/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Board Linux gen3/include/nic/nic_rx.h: HW_MAC_RX_DESC_T, groups 4/1/2/3,
 * RX_STATUS_HEADER_TRAN, RX_VT_RX_MODE/RATE and RX_VT_RCPI. RCPI conversion:
 * board gen3/include/nic/mac.h RCPI_TO_dBm. */
#include "native_hif_rx.h"
#include "native_frame.h"
static unsigned le16(const uint8_t *p){return (unsigned)p[0]|((unsigned)p[1]<<8);}
static uint32_t le32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
NDIS_STATUS a04_native_hif_rx_decode(const uint8_t *p,ULONG n,unsigned allowed,
 const uint8_t local[6],const uint8_t ap[6],void *context,a04_native_rate_lookup lookup,struct a04_native_hif_rx *out)
{
 struct a04_native_frame f;struct a04_native_hif_rx temp;unsigned kind,groups,g3,offset,channel,rcpi;uint32_t vector;
 if(!p||!out||!local||!ap||!lookup||n<16||n>4096||le16(p)!=n)return NDIS_STATUS_INVALID_DATA;
 {unsigned i,local_any=0,ap_any=0;if((local[0]&1u)||(ap[0]&1u))return NDIS_STATUS_INVALID_DATA;
  for(i=0;i<6;i++){local_any|=local[i];ap_any|=ap[i];}if(!local_any||!ap_any)return NDIS_STATUS_INVALID_DATA;}
 kind=le16(p+2);groups=(kind>>9)&15u;
 if((kind>>13)!=2u || !(groups&4u) || (p[6]&0x80u) || (p[9]&0xf0u) || (le16(p+10)&0x0ffeu))return NDIS_STATUS_NOT_SUPPORTED;
 channel=p[5];if(!channel||channel>14||!(allowed&(1u<<channel)))return NDIS_STATUS_NOT_SUPPORTED;
 g3=16u+((groups&8u)?16u:0u)+((groups&1u)?16u:0u)+((groups&2u)?8u:0u);
 if(g3>n || 24u>n-g3)return NDIS_STATUS_INVALID_LENGTH;
 offset=g3+24u+((p[6]&0x40u)?2u:0u);
 if(offset>n || a04_native_frame_parse(p+offset,n-offset,2,&f) || (p[6]&63u)!=f.header)return NDIS_STATUS_INVALID_LENGTH;
 if((p[offset+1]&0x40)||(p[7]&3)||(le16(p+10)&0xd000u)||(f.qos&&(p[9]&15)!=f.tid))return NDIS_STATUS_NOT_SUPPORTED;
 {unsigned i;for(i=0;i<6;i++)if((!(p[offset+4]&1u) && p[offset+4+i]!=local[i]) || p[offset+10+i]!=ap[i])return NDIS_STATUS_NOT_SUPPORTED;}
 vector=le32(p+g3);rcpi=p[g3+9];if(rcpi>220u)return NDIS_STATUS_NOT_SUPPORTED;
 NdisZeroMemory(&temp,sizeof(temp));
 if(lookup(context,(vector>>12)&7u,vector&0x7fu,&temp.info.phy,&temp.info.rate))return NDIS_STATUS_NOT_SUPPORTED;
 temp.mpdu=p+offset;temp.length=n-offset;temp.header=f.header;temp.tid=f.tid;temp.info.priority=f.qos?f.tid:0;temp.info.observed=A04_RX_OBS_REQUIRED;
 temp.info.frequency=(channel==14?2484u:2407u+5u*channel)*1000u;temp.info.fragments=1;temp.info.rssi=(LONG)(rcpi>>1)-110;
 *out=temp;return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS a04_native_hif_rx_decode_security(const uint8_t *p,ULONG n,unsigned allowed,
 const uint8_t local[6],const uint8_t ap[6],void *context,a04_native_rate_lookup lookup,struct a04_native_hif_rx *out)
{
 struct a04_native_frame f;struct a04_native_hif_rx temp;unsigned kind,groups,g1,g3,offset,cipher,channel;uint32_t vector;
 if(!p||!out||!local||!ap||!lookup||n<16||n>4096||le16(p)!=n)return NDIS_STATUS_INVALID_DATA;
 kind=le16(p+2);groups=(kind>>9)&15;cipher=p[9]>>4;channel=p[5];
 if((kind>>13)!=2||!(groups&4)||(p[6]&0x80)||(le16(p+10)&0x0ffa)||!channel||channel>13||!(allowed&(1u<<channel))||(cipher!=0&&cipher!=4))return NDIS_STATUS_NOT_SUPPORTED;
 if(cipher==4&&(!(groups&1)||(le16(p+10)&4)))return NDIS_STATUS_NOT_SUPPORTED;
 g1=16+((groups&8)?16:0);g3=g1+((groups&1)?16:0)+((groups&2)?8:0);
 if(g3>n||n-g3<24)return NDIS_STATUS_INVALID_LENGTH;
 offset=g3+24+((p[6]&0x40)?2:0);
 if(offset>n||a04_native_frame_parse(p+offset,n-offset,2,&f)||(p[6]&63)!=f.header||(p[7]&3)||(le16(p+10)&0xd000)||(f.qos&&(p[9]&15)!=f.tid))return NDIS_STATUS_NOT_SUPPORTED;
 for(unsigned i=0;i<6;i++)if((!(p[offset+4]&1)&&p[offset+4+i]!=local[i])||p[offset+10+i]!=ap[i])return NDIS_STATUS_NOT_SUPPORTED;
 if(!cipher&&(p[offset+1]&0x40))return NDIS_STATUS_NOT_SUPPORTED;
 vector=le32(p+g3);if(p[g3+9]>220)return NDIS_STATUS_NOT_SUPPORTED;
 NdisZeroMemory(&temp,sizeof(temp));if(lookup(context,(vector>>12)&7,vector&127,&temp.info.phy,&temp.info.rate))return NDIS_STATUS_NOT_SUPPORTED;
 temp.mpdu=p+offset;temp.length=n-offset;temp.cipher=cipher;temp.key=p[4]>>6;temp.wlan=p[8];temp.tid=f.tid;temp.header=f.header;temp.info.priority=f.qos?f.tid:0;
 if(cipher)for(unsigned i=0;i<6;i++)temp.pn|=(uint64_t)p[g1+i]<<(8*i);
 temp.info.observed=A04_RX_OBS_REQUIRED;temp.info.frequency=(2407+5*channel)*1000;temp.info.fragments=1;temp.info.rssi=(LONG)(p[g3+9]>>1)-110;*out=temp;return NDIS_STATUS_SUCCESS;
}

NDIS_STATUS a04_native_hif_rx_indicate(NDIS_HANDLE adapter,PNET_BUFFER_LIST nbl,const uint8_t *p,ULONG n,unsigned allowed,
 const uint8_t local[6],const uint8_t ap[6],void *context,a04_native_rate_lookup lookup)
{
 struct a04_native_hif_rx rx;NDIS_STATUS status;uint8_t normalized[A04_NATIVE_MPDU_MAX];
 status=a04_native_hif_rx_decode(p,n,allowed,local,ap,context,lookup,&rx);
 if(status!=NDIS_STATUS_SUCCESS)return status;
 /* Standalone helper has no association-scoped retry owner. Runtime uses
  * decode+native_retry+owned slot instead; do not bypass that boundary. */
 if(rx.mpdu[1]&8)return NDIS_STATUS_NOT_SUPPORTED;
 ULONG length=(ULONG)a04_native_frame_copy(normalized,rx.mpdu,rx.length,rx.header);
 if(!nbl||!NET_BUFFER_LIST_FIRST_NB(nbl))return NDIS_STATUS_INVALID_DATA;
 PNET_BUFFER nb=NET_BUFFER_LIST_FIRST_NB(nbl);ULONG previous=NET_BUFFER_DATA_LENGTH(nb);
 if(previous<length)return NDIS_STATUS_INVALID_LENGTH;
 NET_BUFFER_DATA_LENGTH(nb)=length;status=a04_native_indicate_mpdu(adapter,nbl,normalized,length,&rx.info);
 NET_BUFFER_DATA_LENGTH(nb)=previous;return status;
}

/* Same actual RX descriptor/vector contract as data, but management MAC
 * headers do not require an associated AP or contain an LLC/SNAP body. */
NDIS_STATUS a04_native_hif_rx_management(const uint8_t *p,ULONG n,unsigned allowed,
 void *context,a04_native_rate_lookup lookup,struct a04_native_hif_rx *out)
{
 struct a04_native_hif_rx v;unsigned groups,g3,offset,channel;uint32_t vector;
 if(!p||!lookup||!out||n<16||n>4096||le16(p)!=n)return NDIS_STATUS_INVALID_DATA;
 groups=(le16(p+2)>>9)&15;channel=p[5];
 if((le16(p+2)>>13)!=2||!(groups&4)||(p[6]&0x80)||(p[9]&0xf0)||(le16(p+10)&0x0ffe)||!channel||channel>14||!(allowed&(1u<<channel)))return NDIS_STATUS_NOT_SUPPORTED;
 g3=16+((groups&8)?16:0)+((groups&1)?16:0)+((groups&2)?8:0);
 if(g3>n||n-g3<24)return NDIS_STATUS_INVALID_LENGTH;
 offset=g3+24+((p[6]&0x40)?2:0);
 if(offset>n||n-offset<24||(p[6]&63)!=24||(p[offset]&0x0c)||(p[offset+1]&0x43)||(p[offset+22]&15))return NDIS_STATUS_NOT_SUPPORTED;
 vector=le32(p+g3);if(p[g3+9]>220)return NDIS_STATUS_NOT_SUPPORTED;
 NdisZeroMemory(&v,sizeof(v));if(lookup(context,(vector>>12)&7,vector&127,&v.info.phy,&v.info.rate))return NDIS_STATUS_NOT_SUPPORTED;
 v.mpdu=p+offset;v.length=n-offset;v.info.observed=A04_RX_OBS_REQUIRED;v.info.frequency=(channel==14?2484:2407+5*channel)*1000;
 v.info.fragments=1;v.info.rssi=(LONG)(p[g3+9]>>1)-110;*out=v;return NDIS_STATUS_SUCCESS;
}
