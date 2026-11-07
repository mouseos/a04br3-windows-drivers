/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Standard OIDs backed by adapter-owned policy, actual completion counters
 * and observed association state. No fabricated firmware/crypto statistics. */
#include "ndis_runtime.h"
#define HEAD(v,t) do{(v).Header.Type=NDIS_OBJECT_TYPE_DEFAULT;(v).Header.Revision=1;(v).Header.Size=sizeof(t);}while(0)
static NDIS_STATUS query(PNDIS_OID_REQUEST q,const void *p,ULONG n)
{
 q->DATA.QUERY_INFORMATION.BytesWritten=0;q->DATA.QUERY_INFORMATION.BytesNeeded=n;
 if(q->DATA.QUERY_INFORMATION.InformationBufferLength<n)return NDIS_STATUS_BUFFER_TOO_SHORT;
 if(n)NdisMoveMemory(q->DATA.QUERY_INFORMATION.InformationBuffer,p,n);
 q->DATA.QUERY_INFORMATION.BytesWritten=n;q->DATA.QUERY_INFORMATION.BytesNeeded=0;return NDIS_STATUS_SUCCESS;
}
static int header(const void *p,ULONG n,USHORT size)
{NDIS_OBJECT_HEADER h;if(n<sizeof(h))return 0;NdisMoveMemory(&h,p,sizeof(h));return h.Type==NDIS_OBJECT_TYPE_DEFAULT&&h.Revision==1&&h.Size==size;}
static ULONGLONG sum(const ULONGLONG p[3]){return p[0]+p[1]+p[2];}
static int any_bssid(const UCHAR *p){unsigned all=255;for(unsigned i=0;i<6;i++)all&=p[i];return all==255;}
static NDIS_STATUS disconnect_excluded(struct a04_ndis_runtime *a)
{
 if(a->radio.connected&&a04_ndis_excluded(a,a->radio.station.ap.mac)){
  DOT11_DISASSOCIATION_PARAMETERS d;NdisZeroMemory(&d,sizeof(d));HEAD(d,DOT11_DISASSOCIATION_PARAMETERS);
  NdisMoveMemory(d.MacAddr,a->radio.station.ap.mac,6);d.uReason=DOT11_ASSOC_STATUS_DISASSOCIATED_BY_OS;
  if(a04_ndis_radio_reset(a))return NDIS_STATUS_FAILURE;
  a04_ndis_status(a,NDIS_STATUS_DOT11_DISASSOCIATION,&d,sizeof(d));
 }
 return NDIS_STATUS_SUCCESS;
}
NDIS_STATUS a04_ndis_contract_oid(struct a04_ndis_runtime *a,PNDIS_OID_REQUEST q)
{
 ULONG oid=q->DATA.QUERY_INFORMATION.Oid;
 if(q->RequestType==NdisRequestSetInformation){
  UCHAR *p=q->DATA.SET_INFORMATION.InformationBuffer;ULONG n=q->DATA.SET_INFORMATION.InformationBufferLength,value=0;NDIS_STATUS status=NDIS_STATUS_SUCCESS;
  q->DATA.SET_INFORMATION.BytesRead=q->DATA.SET_INFORMATION.BytesNeeded=0;
  switch(oid){
  case OID_DOT11_EXCLUDED_MAC_ADDRESS_LIST:{
   DOT11_MAC_ADDRESS_LIST h;ULONG base=FIELD_OFFSET(DOT11_MAC_ADDRESS_LIST,MacAddrs);DOT11_MAC_ADDRESS entries[A04_NATIVE_EXCLUDED];
   if(n<base){q->DATA.SET_INFORMATION.BytesNeeded=base;return NDIS_STATUS_INVALID_LENGTH;}
   NdisZeroMemory(&h,sizeof(h));NdisMoveMemory(&h,p,base);
   if(!header(p,n,sizeof(h))||h.uNumOfEntries>A04_NATIVE_EXCLUDED||h.uTotalNumOfEntries!=h.uNumOfEntries)return NDIS_STATUS_INVALID_DATA;
   if(h.uNumOfEntries>(n-base)/6){q->DATA.SET_INFORMATION.BytesNeeded=base+6*h.uNumOfEntries;return NDIS_STATUS_INVALID_LENGTH;}
   NdisMoveMemory(entries,p+base,h.uNumOfEntries*6);
   for(ULONG i=0;i<h.uNumOfEntries;i++)if((entries[i][0]&1)&&!any_bssid(entries[i]))return NDIS_STATUS_INVALID_DATA;
   NdisMoveMemory(a->excluded,entries,h.uNumOfEntries*6);a->excluded_count=h.uNumOfEntries;
   for(unsigned i=0;i<a->radio.bss_count;)if(a04_ndis_excluded(a,a->radio.bss[i].ap.mac))a->radio.bss[i]=a->radio.bss[--a->radio.bss_count];else i++;
   status=disconnect_excluded(a);break;
  }
  case OID_DOT11_MULTICAST_LIST:
   if(n%6)return NDIS_STATUS_INVALID_LENGTH;
   if(n/6>A04_NATIVE_MULTICAST)return NDIS_STATUS_MULTICAST_FULL;
   for(ULONG i=0;i<n;i+=6)if(!(p[i]&1)||any_bssid(p+i))return NDIS_STATUS_INVALID_DATA;
   NdisMoveMemory(a->multicast,p,n);a->multicast_count=n/6;break;
  case OID_DOT11_ASSOCIATION_PARAMS:{
   DOT11_ASSOCIATION_PARAMS v;if(n<sizeof(v))return NDIS_STATUS_INVALID_LENGTH;NdisMoveMemory(&v,p,sizeof(v));
   if(!header(p,n,sizeof(v))||v.uAssocRequestIEsOffset>n||v.uAssocRequestIEsLength>n-v.uAssocRequestIEsOffset||v.uAssocRequestIEsLength>256||(v.uAssocRequestIEsLength&&v.uAssocRequestIEsOffset<sizeof(v)))return NDIS_STATUS_INVALID_DATA;
   if(a->radio.connected)return NDIS_STATUS_DOT11_MEDIA_IN_USE;
   const UCHAR *ies=p+v.uAssocRequestIEsOffset;ULONG length=v.uAssocRequestIEsLength;
   if(a04_wlan_ies(ies,length))return NDIS_STATUS_INVALID_DATA;
   for(ULONG i=0;i<length;i+=2+ies[i+1]){
    /* OS selection must agree with the existing RSNA-PSK/CCMP builder.
     * Do not permit arbitrary extra IEs to override security/SSID/rates. */
    if(ies[i]==0||ies[i]==1||ies[i]==50||ies[i]==48||
      (ies[i]==221&&ies[i+1]>=4&&ies[i+2]==0&&ies[i+3]==0x50&&ies[i+4]==0xf2&&ies[i+5]==1))return NDIS_STATUS_NOT_SUPPORTED;
   }
   if(!a04_wlan_mac_valid(v.BSSID)&&!any_bssid(v.BSSID))return NDIS_STATUS_INVALID_DATA;
   NdisMoveMemory(a->assoc_ies,ies,length);a->assoc_ies_length=length;NdisMoveMemory(a->assoc_bssid,v.BSSID,6);break;
  }
  case OID_DOT11_HIDDEN_NETWORK_ENABLED:case OID_DOT11_MEDIA_STREAMING_ENABLED:case OID_DOT11_SAFE_MODE_ENABLED:
   if(n!=sizeof(BOOLEAN))return NDIS_STATUS_INVALID_LENGTH;if(*p>1)return NDIS_STATUS_INVALID_DATA;
   if(oid==OID_DOT11_HIDDEN_NETWORK_ENABLED)a->hidden_network=*p;
   else if(oid==OID_DOT11_MEDIA_STREAMING_ENABLED){if(a04_ndis_power_profile(a,a->power_request,*p,0))return NDIS_STATUS_FAILURE;LARGE_INTEGER due;due.QuadPart=-10000*(a->media_streaming?5:10);KeSetTimerEx(&a->timer,due,a->media_streaming?5:10,&a->dpc);}
   else{if(*p&&a->radio.connected&&a->os_security.auth!=DOT11_AUTH_ALGO_RSNA_PSK)return NDIS_STATUS_DOT11_MEDIA_IN_USE;a->safe_mode=*p;}break;
  case OID_DOT11_NIC_POWER_STATE:
   if(n!=sizeof(BOOLEAN))return NDIS_STATUS_INVALID_LENGTH;if(*p>1)return NDIS_STATUS_INVALID_DATA;
   {
    BOOLEAN before=a->power;BOOLEAN connected=(BOOLEAN)a->radio.connected;DOT11_MAC_ADDRESS peer;
    NdisMoveMemory(peer,a->radio.station.ap.mac,6);
    status=a04_ndis_radio_power(a,*p)?NDIS_STATUS_FAILURE:NDIS_STATUS_SUCCESS;
    if(before!=a->power){
     DOT11_PHY_STATE_PARAMETERS v;NdisZeroMemory(&v,sizeof(v));HEAD(v,DOT11_PHY_STATE_PARAMETERS);
     v.uPhyId=0;v.bHardwarePhyState=TRUE;v.bSoftwarePhyState=a->power;
     a04_ndis_status(a,NDIS_STATUS_DOT11_PHY_STATE_CHANGED,&v,sizeof(v));
     if(!a->power&&connected){DOT11_DISASSOCIATION_PARAMETERS d;NdisZeroMemory(&d,sizeof(d));HEAD(d,DOT11_DISASSOCIATION_PARAMETERS);
      NdisMoveMemory(d.MacAddr,peer,6);d.uReason=DOT11_ASSOC_STATUS_RADIO_OFF;
      a04_ndis_status(a,NDIS_STATUS_DOT11_DISASSOCIATION,&d,sizeof(d));}
    }
   }break;
  case OID_DOT11_UNREACHABLE_DETECTION_THRESHOLD:
   if(n!=sizeof(value)){q->DATA.SET_INFORMATION.BytesNeeded=sizeof(value);return NDIS_STATUS_INVALID_LENGTH;}NdisMoveMemory(&value,p,sizeof(value));
   a->peer_loss.threshold_ms=value;
   status=a04_ndis_unreachable_check(a)?NDIS_STATUS_FAILURE:NDIS_STATUS_SUCCESS;break;
  case OID_DOT11_POWER_MGMT_REQUEST:
   if(n!=sizeof(value))return NDIS_STATUS_INVALID_LENGTH;NdisMoveMemory(&value,p,4);
   unsigned profile;int valid=a04_native_power_profile(value,&profile);
   if(valid)return valid==-2?NDIS_STATUS_NOT_SUPPORTED:NDIS_STATUS_INVALID_DATA;
   if(a04_ndis_power_profile(a,value,a->media_streaming,0))return NDIS_STATUS_FAILURE;break;
  default:return NDIS_STATUS_INVALID_OID;
  }
  q->DATA.SET_INFORMATION.BytesRead=status==NDIS_STATUS_SUCCESS?n:0;q->DATA.SET_INFORMATION.BytesNeeded=0;return status;
 }
 if(q->RequestType!=NdisRequestQueryInformation&&q->RequestType!=NdisRequestQueryStatistics)return NDIS_STATUS_INVALID_OID;
 struct a04_runtime_stats counts;NdisAcquireSpinLock(&a->lock);counts=a->stats;NdisReleaseSpinLock(&a->lock);
 ULONGLONG count=0;ULONG value=0;
 switch(oid){
 case OID_DOT11_MULTICAST_LIST:return query(q,a->multicast,a->multicast_count*6);
 case OID_DOT11_MAXIMUM_LIST_SIZE:value=A04_NATIVE_MULTICAST;break;
 case OID_DOT11_NIC_POWER_STATE:case OID_DOT11_HIDDEN_NETWORK_ENABLED:case OID_DOT11_MEDIA_STREAMING_ENABLED:case OID_DOT11_SAFE_MODE_ENABLED:{
  BOOLEAN v=oid==OID_DOT11_NIC_POWER_STATE?a->power:(oid==OID_DOT11_HIDDEN_NETWORK_ENABLED?a->hidden_network:(oid==OID_DOT11_MEDIA_STREAMING_ENABLED?a->media_streaming:a->safe_mode));return query(q,&v,sizeof(v));
 }
 case OID_DOT11_UNREACHABLE_DETECTION_THRESHOLD:value=a->peer_loss.threshold_ms;break;
 case OID_DOT11_POWER_MGMT_REQUEST:value=a->power_request;break;
 case OID_DOT11_ACTIVE_PHY_LIST:{DOT11_PHY_ID_LIST v;NdisZeroMemory(&v,sizeof(v));HEAD(v,DOT11_PHY_ID_LIST);v.uNumOfEntries=v.uTotalNumOfEntries=a->power?1:0;v.dot11PhyId[0]=0;return query(q,&v,FIELD_OFFSET(DOT11_PHY_ID_LIST,dot11PhyId)+v.uNumOfEntries*sizeof(ULONG));}
 case OID_DOT11_EXCLUDED_MAC_ADDRESS_LIST:{
  union{DOT11_MAC_ADDRESS_LIST h;UCHAR bytes[sizeof(DOT11_MAC_ADDRESS_LIST)+6*A04_NATIVE_EXCLUDED];} v;
  NdisZeroMemory(&v,sizeof(v));HEAD(v.h,DOT11_MAC_ADDRESS_LIST);v.h.uNumOfEntries=v.h.uTotalNumOfEntries=a->excluded_count;
  NdisMoveMemory(v.h.MacAddrs,a->excluded,6*a->excluded_count);return query(q,&v,FIELD_OFFSET(DOT11_MAC_ADDRESS_LIST,MacAddrs)+6*a->excluded_count);
 }
 case OID_DOT11_ENUM_ASSOCIATION_INFO:{
  DOT11_ASSOCIATION_INFO_LIST v;NdisZeroMemory(&v,sizeof(v));HEAD(v,DOT11_ASSOCIATION_INFO_LIST);
  if(a->radio.connected){DOT11_ASSOCIATION_INFO_EX *i=&v.dot11AssocInfo[0];v.uNumOfEntries=v.uTotalNumOfEntries=1;
   NdisMoveMemory(i->PeerMacAddress,a->radio.station.ap.mac,6);NdisMoveMemory(i->BSSID,i->PeerMacAddress,6);
   i->usCapabilityInformation=(USHORT)a->radio.station.ap.capability;i->usListenInterval=10;i->usAssociationID=(USHORT)a->radio.station.join.aid;
   i->dot11AssociationState=dot11_assoc_state_auth_assoc;i->dot11PowerMode=(!a->media_streaming&&a->power_request)?dot11_power_mode_powersave:dot11_power_mode_active;
   NdisMoveMemory(i->ucPeerSupportedRates,a->radio.station.ap.rates,a->radio.station.ap.rate_count);
   i->liAssociationUpTime.QuadPart=(KeQueryInterruptTime()/10u-a->association_started)*10u;
   i->ullNumOfTxPacketSuccesses=sum(counts.tx_packets)-sum(a->association_base.tx_packets);i->ullNumOfTxPacketFailures=counts.tx_errors-a->association_base.tx_errors;
   i->ullNumOfRxPacketSuccesses=sum(counts.rx_packets)-sum(a->association_base.rx_packets);i->ullNumOfRxPacketFailures=counts.rx_errors-a->association_base.rx_errors;
  }
  return query(q,&v,FIELD_OFFSET(DOT11_ASSOCIATION_INFO_LIST,dot11AssocInfo)+v.uNumOfEntries*sizeof(v.dot11AssocInfo[0]));
 }
 case OID_GEN_STATISTICS:{
  NDIS_STATISTICS_INFO v;NdisZeroMemory(&v,sizeof(v));HEAD(v,NDIS_STATISTICS_INFO);v.Header.Size=NDIS_SIZEOF_STATISTICS_INFO_REVISION_1;
  v.SupportedStatistics=A04_HOST_STATS_FLAGS;
  v.ifInErrors=counts.rx_errors;v.ifInDiscards=counts.rx_discards+counts.rx_errors;v.ifOutErrors=counts.tx_errors;v.ifOutDiscards=counts.tx_discards;
  v.ifHCInOctets=sum(counts.rx_bytes);v.ifHCOutOctets=sum(counts.tx_bytes);
  v.ifHCInUcastPkts=counts.rx_packets[0];v.ifHCInMulticastPkts=counts.rx_packets[1];v.ifHCInBroadcastPkts=counts.rx_packets[2];
  v.ifHCOutUcastPkts=counts.tx_packets[0];v.ifHCOutMulticastPkts=counts.tx_packets[1];v.ifHCOutBroadcastPkts=counts.tx_packets[2];
  v.ifHCInUcastOctets=counts.rx_bytes[0];v.ifHCInMulticastOctets=counts.rx_bytes[1];v.ifHCInBroadcastOctets=counts.rx_bytes[2];
  v.ifHCOutUcastOctets=counts.tx_bytes[0];v.ifHCOutMulticastOctets=counts.tx_bytes[1];v.ifHCOutBroadcastOctets=counts.tx_bytes[2];return query(q,&v,sizeof(v));
 }
 case OID_GEN_XMIT_OK:count=sum(counts.tx_packets);goto counter;
 case OID_GEN_RCV_OK:count=sum(counts.rx_packets);goto counter;
 case OID_GEN_XMIT_ERROR:count=counts.tx_errors;goto counter;
 case OID_GEN_RCV_ERROR:count=counts.rx_errors;goto counter;
 case OID_GEN_RCV_NO_BUFFER:count=counts.rx_no_buffer;goto counter;
 case OID_GEN_TRANSMIT_QUEUE_LENGTH:NdisAcquireSpinLock(&a->lock);value=a->send_queue.count+a->send_queue.inflight;NdisReleaseSpinLock(&a->lock);break;
 case OID_DOT11_STATISTICS:{
  DOT11_STATISTICS v;NdisFillMemory(&v,sizeof(v),255);HEAD(v,DOT11_STATISTICS);v.ullReserved=0;
  /* Unknown hardware retry/FCS/MIC/replay and OS handshake counters stay
   * DOT11_STATISTICS_UNKNOWN, not zero. Only these host events are observed. */
  v.MacUcastCounters.ullTransmittedFrameCount=counts.tx_packets[0];v.MacUcastCounters.ullReceivedFrameCount=counts.rx_packets[0];
  v.MacMcastCounters.ullTransmittedFrameCount=counts.tx_packets[1]+counts.tx_packets[2];v.MacMcastCounters.ullReceivedFrameCount=counts.rx_packets[1]+counts.rx_packets[2];
  return query(q,&v,sizeof(v));
 }
 case OID_GEN_INTERRUPT_MODERATION:{NDIS_INTERRUPT_MODERATION_PARAMETERS v;NdisZeroMemory(&v,sizeof(v));HEAD(v,NDIS_INTERRUPT_MODERATION_PARAMETERS);v.InterruptModeration=NdisInterruptModerationNotSupported;return query(q,&v,sizeof(v));}
 default:return NDIS_STATUS_INVALID_OID;
 }
 return query(q,&value,sizeof(value));
counter:
 /* Legacy callers may supply only32 bits; BytesNeeded still describes64. */
 if(q->DATA.QUERY_INFORMATION.InformationBufferLength>=sizeof(count))return query(q,&count,sizeof(count));
 value=(ULONG)count;NDIS_STATUS status=query(q,&value,sizeof(value));if(status==NDIS_STATUS_SUCCESS)q->DATA.QUERY_INFORMATION.BytesNeeded=sizeof(count);return status;
}
