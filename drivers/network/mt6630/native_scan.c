/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * DOT11_SCAN_REQUEST_V2 offsets are relative to ucBuffer, per Microsoft WDK:
 * https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/windot11/ns-windot11-_dot11_scan_request_v2
 */
#include "native_scan.h"
#include <stddef.h>
static int span(ULONG total,ULONG offset,ULONG bytes,ULONG base,ULONG *needed)
{
 if(offset<=total && bytes<=total-offset)return 1;
 if(offset<=0xffffffffu-base && bytes<=0xffffffffu-base-offset)*needed=base+offset+bytes;
 else *needed=0xffffffffu;
 return 0;
}
NDIS_STATUS a04_native_scan_parse(const void *buffer,ULONG length,struct a04_native_scan_request *out,ULONG *needed)
{
 DOT11_SCAN_REQUEST_V2 h;DOT11_SSID ssid;struct a04_native_scan_request temp;
 const UCHAR *raw=buffer,*payload;ULONG base=(ULONG)offsetof(DOT11_SCAN_REQUEST_V2,ucBuffer),available,i,type,zero=0,ones=0;
 if(!out || !needed)return NDIS_STATUS_INVALID_DATA;
 *needed=base;
 if(!buffer || length<base)return NDIS_STATUS_INVALID_LENGTH;
 NdisZeroMemory(&h,sizeof(h));NdisMoveMemory(&h,buffer,base);NdisZeroMemory(&temp,sizeof(temp));
 payload=raw+base;available=length-base;
 if(h.dot11BSSType!=dot11_BSS_type_infrastructure && h.dot11BSSType!=dot11_BSS_type_any)return NDIS_STATUS_NOT_SUPPORTED;
 /* Both zero and FF:FF:FF:FF:FF:FF request a wildcard scan. */
 for(i=0;i<6;i++){zero|=h.dot11BSSID[i];ones|=h.dot11BSSID[i]^0xffu;}
 if(zero&&ones)return NDIS_STATUS_NOT_SUPPORTED;
 type=(ULONG)h.dot11ScanType;temp.forced=!!(type&0x80000000u);type&=0x7fffffffu;
 if(type!=dot11_scan_type_active && type!=dot11_scan_type_passive && type!=dot11_scan_type_auto)return NDIS_STATUS_INVALID_DATA;
 temp.scan_type=type;
 /* This runtime does not enable multi-domain/802.11d. The Request IE
  * controls are ignored in that mode, as specified for SCAN_REQUEST_V2. */
 if(h.bRestrictedScan || h.uNumOfPhyTypeInfos)return NDIS_STATUS_NOT_SUPPORTED;
 if(h.uNumOfdot11SSIDs>A04_NATIVE_SCAN_SSIDS)return NDIS_STATUS_INVALID_LENGTH;
 if(h.uNumOfdot11SSIDs && !span(available,h.udot11SSIDsOffset,h.uNumOfdot11SSIDs*sizeof(ssid),base,needed))return NDIS_STATUS_INVALID_LENGTH;
 temp.ssid_count=h.uNumOfdot11SSIDs;
 for(i=0;i<temp.ssid_count;i++){
  NdisMoveMemory(&ssid,payload+h.udot11SSIDsOffset+i*sizeof(ssid),sizeof(ssid));
  if(ssid.uSSIDLength>32)return NDIS_STATUS_INVALID_DATA;
  temp.ssid_lengths[i]=ssid.uSSIDLength;NdisMoveMemory(temp.ssids[i],ssid.ucSSID,ssid.uSSIDLength);
 }
 /* Firmware's named wildcard mode cannot prove directed probes for a
  * mixed wildcard/specific list. Reject instead of dropping specific SSIDs. */
 unsigned empty=0;for(i=0;i<temp.ssid_count;i++)if(!temp.ssid_lengths[i])empty++;
 if(empty&&empty<temp.ssid_count)return NDIS_STATUS_NOT_SUPPORTED;
 if(temp.ssid_count){temp.ssid_length=temp.ssid_lengths[0];NdisMoveMemory(temp.ssid,temp.ssids[0],temp.ssid_length);}
 if(h.uIEsLength>sizeof(temp.ie))return NDIS_STATUS_INVALID_LENGTH;
 if(h.uIEsLength && !span(available,h.uIEsOffset,h.uIEsLength,base,needed))return NDIS_STATUS_INVALID_LENGTH;
 if(h.uIEsLength){
  for(i=0;i<h.uIEsLength;){ULONG remain=h.uIEsLength-i,n;
   if(remain<2)return NDIS_STATUS_INVALID_DATA;
   n=payload[h.uIEsOffset+i+1];if(n>remain-2)return NDIS_STATUS_INVALID_DATA;i+=2+n;
  }
  temp.ie_length=h.uIEsLength;NdisMoveMemory(temp.ie,payload+h.uIEsOffset,temp.ie_length);
 }
 *out=temp;*needed=0;return NDIS_STATUS_SUCCESS;
}
