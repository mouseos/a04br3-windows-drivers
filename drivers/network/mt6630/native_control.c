/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "ndis_runtime.h"
#include "station_selection.h"
#define HDR(v,t,rev) do{(v).Header.Type=NDIS_OBJECT_TYPE_DEFAULT;(v).Header.Revision=(rev);(v).Header.Size=sizeof(t);}while(0)
static const NDIS_OID oids[]={OID_GEN_SUPPORTED_LIST,OID_GEN_HARDWARE_STATUS,OID_GEN_MEDIA_SUPPORTED,OID_GEN_MEDIA_IN_USE,OID_GEN_MAXIMUM_FRAME_SIZE,OID_GEN_MAXIMUM_TOTAL_SIZE,OID_GEN_LINK_SPEED,OID_GEN_CURRENT_LOOKAHEAD,OID_GEN_CURRENT_PACKET_FILTER,OID_GEN_VENDOR_ID,OID_GEN_VENDOR_DESCRIPTION,OID_GEN_DRIVER_VERSION,OID_GEN_MEDIA_CONNECT_STATUS,
 OID_DOT11_CIPHER_KEY_MAPPING_KEY,OID_DOT11_CIPHER_DEFAULT_KEY,OID_DOT11_CIPHER_DEFAULT_KEY_ID,OID_DOT11_PORT_STATE_NOTIFICATION,OID_DOT11_RESET_REQUEST,OID_DOT11_CURRENT_ADDRESS,OID_DOT11_PERMANENT_ADDRESS,OID_DOT11_OPERATION_MODE_CAPABILITY,OID_DOT11_CURRENT_OPERATION_MODE,OID_DOT11_SUPPORTED_PHY_TYPES,OID_DOT11_CURRENT_PHY_ID,OID_DOT11_DESIRED_PHY_LIST,OID_DOT11_HARDWARE_PHY_STATE,OID_DOT11_NIC_POWER_STATE,OID_DOT11_EXTSTA_CAPABILITY,OID_DOT11_AUTO_CONFIG_ENABLED,OID_DOT11_DESIRED_BSS_TYPE,OID_DOT11_DESIRED_SSID_LIST,OID_DOT11_DESIRED_BSSID_LIST,OID_DOT11_ENABLED_AUTHENTICATION_ALGORITHM,OID_DOT11_ENABLED_UNICAST_CIPHER_ALGORITHM,OID_DOT11_ENABLED_MULTICAST_CIPHER_ALGORITHM,OID_DOT11_SUPPORTED_UNICAST_ALGORITHM_PAIR,OID_DOT11_SUPPORTED_MULTICAST_ALGORITHM_PAIR,OID_DOT11_PRIVACY_EXEMPTION_LIST,OID_DOT11_EXCLUDE_UNENCRYPTED,OID_DOT11_SCAN_REQUEST,OID_DOT11_ENUM_BSS_LIST,OID_DOT11_FLUSH_BSS_LIST,OID_DOT11_CONNECT_REQUEST,OID_DOT11_DISCONNECT_REQUEST,OID_DOT11_CURRENT_CHANNEL,OID_DOT11_CURRENT_FREQUENCY,OID_DOT11_SUPPORTED_COUNTRY_OR_REGION_STRING,OID_DOT11_DESIRED_COUNTRY_OR_REGION_STRING};
void a04_ndis_status(struct a04_ndis_runtime *a,NDIS_STATUS status,void *data,ULONG bytes)
{NDIS_STATUS_INDICATION i;NdisZeroMemory(&i,sizeof(i));i.Header.Type=NDIS_OBJECT_TYPE_STATUS_INDICATION;i.Header.Revision=1;i.Header.Size=NDIS_SIZEOF_STATUS_INDICATION_REVISION_1;i.SourceHandle=a->adapter;i.StatusCode=status;i.StatusBuffer=data;i.StatusBufferSize=bytes;NdisMIndicateStatusEx(a->adapter,&i);}
NDIS_STATUS a04_ndis_attributes(struct a04_ndis_runtime *a)
{
 NDIS_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES g;NDIS_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES n;DOT11_PHY_ATTRIBUTES phy;DOT11_EXTSTA_ATTRIBUTES sta;
 DOT11_AUTH_CIPHER_PAIR pair[2]={{DOT11_AUTH_ALGO_80211_OPEN,DOT11_CIPHER_ALGO_NONE},{DOT11_AUTH_ALGO_RSNA_PSK,DOT11_CIPHER_ALGO_CCMP}};DOT11_COUNTRY_OR_REGION_STRING country;NDIS_STATUS s;
 static const UCHAR rates[12]={2,4,11,22,12,18,24,36,48,72,96,108};
 NdisZeroMemory(&g,sizeof(g));g.Header.Type=NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES;g.Header.Revision=1;g.Header.Size=NDIS_SIZEOF_MINIPORT_ADAPTER_GENERAL_ATTRIBUTES_REVISION_1;
 g.MediaType=NdisMediumNative802_11;g.PhysicalMediumType=NdisPhysicalMediumNative802_11;g.MtuSize=1500;g.MaxXmitLinkSpeed=g.MaxRcvLinkSpeed=54000000;g.XmitLinkSpeed=g.RcvLinkSpeed=NDIS_LINK_SPEED_UNKNOWN;g.MediaConnectState=MediaConnectStateDisconnected;g.MediaDuplexState=MediaDuplexStateHalf;g.LookaheadSize=1500;g.MacOptions=NDIS_MAC_OPTION_TRANSFERS_NOT_PEND|NDIS_MAC_OPTION_NO_LOOPBACK;
 g.SupportedPacketFilters=NDIS_PACKET_TYPE_DIRECTED|NDIS_PACKET_TYPE_BROADCAST|NDIS_PACKET_TYPE_MULTICAST;g.MaxMulticastListSize=0;g.MacAddressLength=6;NdisMoveMemory(g.PermanentMacAddress,a->radio.bridge->regulatory.mac,6);NdisMoveMemory(g.CurrentMacAddress,g.PermanentMacAddress,6);
 g.AccessType=NET_IF_ACCESS_BROADCAST;g.DirectionType=NET_IF_DIRECTION_SENDRECEIVE;g.ConnectionType=NET_IF_CONNECTION_DEDICATED;g.IfType=IF_TYPE_IEEE80211;g.IfConnectorPresent=TRUE;g.SupportedOidList=(PNDIS_OID)oids;g.SupportedOidListLength=sizeof(oids);
 s=NdisMSetMiniportAttributes(a->adapter,(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&g);if(s!=NDIS_STATUS_SUCCESS)return s;
 NdisZeroMemory(&phy,sizeof(phy));HDR(phy,DOT11_PHY_ATTRIBUTES,1);phy.PhyType=dot11_phy_type_erp;phy.bHardwarePhyState=phy.bSoftwarePhyState=TRUE;phy.uMPDUMaxLength=2304;phy.TempType=dot11_temp_type_unknown;phy.DiversitySupport=dot11_diversity_support_unknown;
 phy.ERPAttributes.bShortPreambleOptionImplemented=TRUE;phy.ERPAttributes.bPBCCOptionImplemented=FALSE;phy.ERPAttributes.bShortSlotTimeOptionImplemented=TRUE;
 phy.uNumDataRateMappingEntries=12;
 for(ULONG i=0;i<12;i++){phy.DataRateMappingEntries[i].ucDataRateIndex=rates[i];phy.DataRateMappingEntries[i].usDataRateValue=(USHORT)rates[i];phy.SupportedDataRatesValue.ucSupportedTxDataRatesValue[i]=rates[i];phy.SupportedDataRatesValue.ucSupportedRxDataRatesValue[i]=rates[i];}
 NdisZeroMemory(&sta,sizeof(sta));HDR(sta,DOT11_EXTSTA_ATTRIBUTES,1);sta.uScanSSIDListSize=sta.uDesiredBSSIDListSize=sta.uDesiredSSIDListSize=1;sta.uKeyMappingTableSize=1;sta.uDefaultKeyTableSize=4;sta.uPrivacyExemptionListSize=4;sta.uInfraNumSupportedUcastAlgoPairs=sta.uInfraNumSupportedMcastAlgoPairs=2;sta.pInfraSupportedUcastAlgoPairs=sta.pInfraSupportedMcastAlgoPairs=pair;
 country[0]=a->radio.bridge->regulatory.country[0];country[1]=a->radio.bridge->regulatory.country[1];country[2]=' ';sta.uNumSupportedCountryOrRegionStrings=1;sta.pSupportedCountryOrRegionStrings=&country;
 NdisZeroMemory(&n,sizeof(n));n.Header.Type=NDIS_OBJECT_TYPE_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES;n.Header.Revision=1;n.Header.Size=NDIS_SIZEOF_MINIPORT_ADAPTER_NATIVE_802_11_ATTRIBUTES_REVISION_1;n.OpModeCapability=DOT11_OPERATION_MODE_EXTENSIBLE_STATION;n.NumOfTXBuffers=n.NumOfRXBuffers=1;n.NumSupportedPhys=1;n.SupportedPhyAttributes=&phy;n.ExtSTAAttributes=&sta;
 return NdisMSetMiniportAttributes(a->adapter,(PNDIS_MINIPORT_ADAPTER_ATTRIBUTES)&n);
}
static void link_state(struct a04_ndis_runtime *a,int connected)
{
 NDIS_LINK_STATE state;NdisZeroMemory(&state,sizeof(state));state.Header.Type=NDIS_OBJECT_TYPE_DEFAULT;state.Header.Revision=1;state.Header.Size=NDIS_SIZEOF_LINK_STATE_REVISION_1;
 state.MediaConnectState=connected?MediaConnectStateConnected:MediaConnectStateDisconnected;state.MediaDuplexState=MediaDuplexStateHalf;state.XmitLinkSpeed=state.RcvLinkSpeed=NDIS_LINK_SPEED_UNKNOWN;
 a04_ndis_status(a,NDIS_STATUS_LINK_STATE,&state,sizeof(state));
}
void a04_ndis_peer_disconnect(struct a04_ndis_runtime *a,ULONG reason)
{
 DOT11_DISASSOCIATION_PARAMETERS v;NdisZeroMemory(&v,sizeof(v));HDR(v,DOT11_DISASSOCIATION_PARAMETERS,1);NdisMoveMemory(v.MacAddr,a->radio.station.ap.mac,6);v.uReason=reason;
 a->os_security.port_open=0;a->radio.connected=0;a->radio.failed=-1;
 a04_os_security_clear(a);link_state(a,0);a04_ndis_status(a,NDIS_STATUS_DOT11_DISASSOCIATION,&v,sizeof(v));
}
static NDIS_STATUS copy_query(PNDIS_OID_REQUEST q,const void *p,ULONG n)
{
 q->DATA.QUERY_INFORMATION.BytesWritten=0;q->DATA.QUERY_INFORMATION.BytesNeeded=n;
 if(q->DATA.QUERY_INFORMATION.InformationBufferLength<n)return NDIS_STATUS_BUFFER_TOO_SHORT;
 if(n)NdisMoveMemory(q->DATA.QUERY_INFORMATION.InformationBuffer,p,n);q->DATA.QUERY_INFORMATION.BytesWritten=n;q->DATA.QUERY_INFORMATION.BytesNeeded=0;return NDIS_STATUS_SUCCESS;
}
static NDIS_STATUS scan(struct a04_ndis_runtime *a,PNDIS_OID_REQUEST q)
{
 struct a04_native_scan_policy policy={0};struct a04_wmt_bridge *b=a->radio.bridge;ULONG needed=0;NDIS_STATUS s;uint64_t end;
 if(a->radio.connected)return NDIS_STATUS_DOT11_MEDIA_IN_USE;
 policy.bss=a->radio.station.local.bss;policy.sequence=(b->scan.sequence+1)&255;policy.probes=2;policy.min_dwell=20;policy.dwell=40;policy.timeout=2000;
 struct a04_native_scan_request parsed;
 s=a04_native_scan_parse(q->DATA.SET_INFORMATION.InformationBuffer,q->DATA.SET_INFORMATION.InformationBufferLength,&parsed,&needed);
 if(s!=NDIS_STATUS_SUCCESS){q->DATA.SET_INFORMATION.BytesNeeded=needed;return s;}
 unsigned channels=parsed.scan_type==dot11_scan_type_active?b->regulatory.active:b->regulatory.permitted;
 for(unsigned ch=1;ch<=13;ch++)if(channels&(1u<<ch)){policy.channels[2*policy.channel_count]=1;policy.channels[2*policy.channel_count+1]=(UCHAR)ch;policy.channel_count++;}
 a->radio.bss_count=0;s=a04_native_scan_start(b,q->DATA.SET_INFORMATION.InformationBuffer,q->DATA.SET_INFORMATION.InformationBufferLength,&policy,&needed);
 q->DATA.SET_INFORMATION.BytesNeeded=needed;if(s!=NDIS_STATUS_SUCCESS)return s;
 q->DATA.SET_INFORMATION.BytesRead=q->DATA.SET_INFORMATION.InformationBufferLength;
 /* OID completes when scan is queued; confirmation follows the real event. */
 NdisAcquireSpinLock(&a->lock);a->request=NULL;NdisReleaseSpinLock(&a->lock);NdisMOidRequestComplete(a->adapter,q,NDIS_STATUS_SUCCESS);
 end=a->radio.now(a)+3000000;
 while(b->scan.pending&&!a->closing&&!a->cancelled&&a->radio.now(a)<end){if(a04_station_runtime_poll(&a->radio))break;a->radio.wait(a,1000);}
 if(b->scan.complete)(void)a04_native_scan_indicate(b,a->adapter);else{(void)a04_native_scan_fail(b,a->adapter,NDIS_STATUS_FAILURE);a->radio.failed=-1;}
 return NDIS_STATUS_PENDING; /* already completed above */
}
static NDIS_STATUS bss_list(struct a04_ndis_runtime *a,PNDIS_OID_REQUEST q)
{
 ULONG total=FIELD_OFFSET(DOT11_BYTE_ARRAY,ucBuffer),offset=0,capacity=q->DATA.METHOD_INFORMATION.OutputBufferLength;DOT11_BYTE_ARRAY *out=q->DATA.METHOD_INFORMATION.InformationBuffer;
 q->DATA.METHOD_INFORMATION.BytesWritten=q->DATA.METHOD_INFORMATION.BytesRead=0;
 for(unsigned i=0;i<a->radio.bss_count;i++)if(a->radio.bss[i].ap.rcpi_valid)total+=FIELD_OFFSET(DOT11_BSS_ENTRY,ucBuffer)+a->radio.bss[i].ie_length;
 q->DATA.METHOD_INFORMATION.BytesNeeded=total;if(capacity<total)return NDIS_STATUS_BUFFER_TOO_SHORT;
 NdisZeroMemory(out,total);HDR(*out,DOT11_BYTE_ARRAY,1);out->Header.Size=FIELD_OFFSET(DOT11_BYTE_ARRAY,ucBuffer);out->uNumOfBytes=out->uTotalNumOfBytes=total-FIELD_OFFSET(DOT11_BYTE_ARRAY,ucBuffer);
 for(unsigned i=0;i<a->radio.bss_count;i++){
  struct a04_bss_observation *v=&a->radio.bss[i];DOT11_BSS_ENTRY value;DOT11_BSS_ENTRY *entry=&value;
  if(!v->ap.rcpi_valid)continue;NdisZeroMemory(entry,sizeof(*entry));entry->uPhyId=0;entry->PhySpecificInfo.uChCenterFrequency=(2407+5*v->ap.channel)*1000;NdisMoveMemory(entry->dot11BSSID,v->ap.mac,6);entry->dot11BSSType=dot11_BSS_type_infrastructure;entry->lRSSI=(LONG)v->ap.rcpi/2-110;entry->uLinkQuality=v->ap.rcpi>160?100:(v->ap.rcpi>60?(v->ap.rcpi-60):0);entry->bInRegDomain=TRUE;entry->usBeaconPeriod=(USHORT)v->ap.beacon;entry->ullTimestamp=v->timestamp;entry->ullHostTimestamp=v->host_timestamp;entry->usCapabilityInformation=(USHORT)v->ap.capability;entry->uBufferLength=v->ie_length;NdisMoveMemory(out->ucBuffer+offset,entry,FIELD_OFFSET(DOT11_BSS_ENTRY,ucBuffer));NdisMoveMemory(out->ucBuffer+offset+FIELD_OFFSET(DOT11_BSS_ENTRY,ucBuffer),v->ies,v->ie_length);offset+=FIELD_OFFSET(DOT11_BSS_ENTRY,ucBuffer)+v->ie_length;
 }
 q->DATA.METHOD_INFORMATION.BytesWritten=total;q->DATA.METHOD_INFORMATION.BytesNeeded=0;return NDIS_STATUS_SUCCESS;
}
static NDIS_STATUS connect(struct a04_ndis_runtime *a,PNDIS_OID_REQUEST q)
{
 DOT11_CONNECTION_START_PARAMETERS start;DOT11_CONNECTION_COMPLETION_PARAMETERS done;DOT11_ASSOCIATION_START_PARAMETERS ast;
 union {DOT11_ASSOCIATION_COMPLETION_PARAMETERS align;uint8_t bytes[sizeof(DOT11_ASSOCIATION_COMPLETION_PARAMETERS)+384+1024+1024+sizeof(ULONG)];} buffer;uint8_t *storage=buffer.bytes;DOT11_ASSOCIATION_COMPLETION_PARAMETERS *ac=&buffer.align;ULONG offset=sizeof(*ac),phy=0;int e;unsigned index;
 unsigned secured=a->os_security.auth==DOT11_AUTH_ALGO_RSNA_PSK;
 if(secured){if(a->os_security.unicast!=DOT11_CIPHER_ALGO_CCMP||a->os_security.multicast!=DOT11_CIPHER_ALGO_CCMP)return NDIS_STATUS_NOT_SUPPORTED;}
 else if(a->os_security.unicast||a->os_security.multicast)return NDIS_STATUS_NOT_SUPPORTED;
 for(index=0;index<a->radio.bss_count;index++)if(a04_station_candidate_matches(&a->radio.station.local,&a->radio.bss[index].ap,a->desired.ucSSID,a->desired.uSSIDLength,secured))break;
 if(index==a->radio.bss_count)return NDIS_STATUS_FAILURE;
 a->radio.desired_secured=secured;a->association_reported=0;
 NdisAcquireSpinLock(&a->lock);a->request=NULL;NdisReleaseSpinLock(&a->lock);NdisMOidRequestComplete(a->adapter,q,NDIS_STATUS_SUCCESS);
 NdisZeroMemory(&start,sizeof(start));HDR(start,DOT11_CONNECTION_START_PARAMETERS,1);start.BSSType=dot11_BSS_type_infrastructure;a04_ndis_status(a,NDIS_STATUS_DOT11_CONNECTION_START,&start,sizeof(start));
 NdisZeroMemory(&ast,sizeof(ast));HDR(ast,DOT11_ASSOCIATION_START_PARAMETERS,1);NdisMoveMemory(ast.MacAddr,a->radio.bss[index].ap.mac,6);ast.SSID=a->desired;a04_ndis_status(a,NDIS_STATUS_DOT11_ASSOCIATION_START,&ast,sizeof(ast));
 e=a04_station_runtime_connect(&a->radio,a->desired.ucSSID,a->desired.uSSIDLength);
 NdisZeroMemory(storage,sizeof(buffer));HDR(*ac,DOT11_ASSOCIATION_COMPLETION_PARAMETERS,1);NdisMoveMemory(ac->MacAddr,ast.MacAddr,6);ac->uStatus=e?DOT11_ASSOC_STATUS_FAILURE:DOT11_ASSOC_STATUS_SUCCESS;
 if(!e){
  ac->AuthAlgo=secured?DOT11_AUTH_ALGO_RSNA_PSK:DOT11_AUTH_ALGO_80211_OPEN;ac->UnicastCipher=ac->MulticastCipher=secured?DOT11_CIPHER_ALGO_CCMP:DOT11_CIPHER_ALGO_NONE;ac->bPortAuthorized=(BOOLEAN)!secured;ac->DSInfo=DOT11_DS_CHANGED;
  /* Native completion contains the frame bodies, not HIF/MAC headers. */
  ac->uAssocReqOffset=offset;ac->uAssocReqSize=a->radio.assoc_request_length-24;NdisMoveMemory(storage+offset,a->radio.assoc_request+24,ac->uAssocReqSize);offset+=ac->uAssocReqSize;
  ac->uAssocRespOffset=offset;ac->uAssocRespSize=a->radio.assoc_response_length-24;NdisMoveMemory(storage+offset,a->radio.assoc_response+24,ac->uAssocRespSize);offset+=ac->uAssocRespSize;
  ac->uBeaconOffset=offset;ac->uBeaconSize=a->radio.bss[index].frame_length-24;NdisMoveMemory(storage+offset,a->radio.bss[index].frame+24,ac->uBeaconSize);offset+=ac->uBeaconSize;
  ac->uActivePhyListOffset=offset;ac->uActivePhyListSize=sizeof(phy);NdisMoveMemory(storage+offset,&phy,sizeof(phy));offset+=sizeof(phy);
 }
 a04_ndis_status(a,NDIS_STATUS_DOT11_ASSOCIATION_COMPLETION,ac,offset);NdisZeroMemory(&done,sizeof(done));HDR(done,DOT11_CONNECTION_COMPLETION_PARAMETERS,1);done.uStatus=e?DOT11_CONNECTION_STATUS_FAILURE:DOT11_CONNECTION_STATUS_SUCCESS;link_state(a,!e);a04_ndis_status(a,NDIS_STATUS_DOT11_CONNECTION_COMPLETION,&done,sizeof(done));
 if(!e)a04_ndis_association_reported(a);
 return NDIS_STATUS_PENDING; /* association != OS handshake/authorization */
}
static NDIS_STATUS reset_request(struct a04_ndis_runtime *a,PNDIS_OID_REQUEST q)
{
 DOT11_RESET_REQUEST input;DOT11_STATUS_INDICATION output;ULONG any=0;int e;
 q->DATA.METHOD_INFORMATION.BytesRead=q->DATA.METHOD_INFORMATION.BytesWritten=0;
 q->DATA.METHOD_INFORMATION.BytesNeeded=sizeof(output);
 if(q->DATA.METHOD_INFORMATION.InputBufferLength<sizeof(input)||q->DATA.METHOD_INFORMATION.OutputBufferLength<sizeof(output))return NDIS_STATUS_INVALID_LENGTH;
 NdisMoveMemory(&input,q->DATA.METHOD_INFORMATION.InformationBuffer,sizeof(input));
 if(input.dot11ResetType!=dot11_reset_type_phy_and_mac)return NDIS_STATUS_NOT_SUPPORTED;
 for(unsigned i=0;i<6;i++)any|=input.dot11MacAddress[i];
 if(any&&!a04_wlan_equal(input.dot11MacAddress,a->radio.bridge->regulatory.mac,6))return NDIS_STATUS_NOT_SUPPORTED;
 e=a04_ndis_radio_reset(a);
 if(!e&&input.bSetDefaultMIB){NdisZeroMemory(&a->desired,sizeof(a->desired));a->filter=0;a->lookahead=1500;a04_os_security_clear(a);}
 output.uStatusType=DOT11_STATUS_RESET_CONFIRM;output.ndisStatus=e?NDIS_STATUS_FAILURE:NDIS_STATUS_SUCCESS;
 NdisMoveMemory(q->DATA.METHOD_INFORMATION.InformationBuffer,&output,sizeof(output));
 q->DATA.METHOD_INFORMATION.BytesRead=sizeof(input);q->DATA.METHOD_INFORMATION.BytesWritten=sizeof(output);q->DATA.METHOD_INFORMATION.BytesNeeded=0;
 link_state(a,0);return e?NDIS_STATUS_FAILURE:NDIS_STATUS_SUCCESS;
}
NDIS_STATUS a04_ndis_control(struct a04_ndis_runtime *a,PNDIS_OID_REQUEST q)
{
 ULONG value=0,oid=q->DATA.QUERY_INFORMATION.Oid;ULONG list_words[32];uint8_t *list=(uint8_t *)list_words;ULONG *words=list_words;
 if((a->radio.failed&&oid!=OID_DOT11_DISCONNECT_REQUEST&&oid!=OID_DOT11_RESET_REQUEST)||!a->power)return NDIS_STATUS_ADAPTER_NOT_READY;
 if(q->RequestType==NdisRequestMethod){if(oid==OID_DOT11_RESET_REQUEST)return reset_request(a,q);if(oid==OID_DOT11_ENUM_BSS_LIST)return bss_list(a,q);return NDIS_STATUS_NOT_SUPPORTED;}
 if(q->RequestType==NdisRequestSetInformation){
  UCHAR *p=q->DATA.SET_INFORMATION.InformationBuffer;ULONG n=q->DATA.SET_INFORMATION.InformationBufferLength;
  q->DATA.SET_INFORMATION.BytesRead=q->DATA.SET_INFORMATION.BytesNeeded=0;
  if(oid==OID_DOT11_CIPHER_KEY_MAPPING_KEY||oid==OID_DOT11_CIPHER_DEFAULT_KEY||oid==OID_DOT11_PORT_STATE_NOTIFICATION)return a04_os_security_oid(a,q);
  if(oid==OID_DOT11_CIPHER_DEFAULT_KEY_ID){if(n!=sizeof(ULONG))return NDIS_STATUS_INVALID_LENGTH;NdisMoveMemory(&value,p,4);if(value>3)return NDIS_STATUS_INVALID_DATA;a->os_security.default_id=value;q->DATA.SET_INFORMATION.BytesRead=4;return NDIS_STATUS_SUCCESS;}
  if(oid==OID_DOT11_SCAN_REQUEST)return scan(a,q);
  if(oid==OID_DOT11_CONNECT_REQUEST)return connect(a,q);
  if(oid==OID_DOT11_DISCONNECT_REQUEST){DOT11_DISASSOCIATION_PARAMETERS v;NdisZeroMemory(&v,sizeof(v));HDR(v,DOT11_DISASSOCIATION_PARAMETERS,1);NdisMoveMemory(v.MacAddr,a->radio.station.ap.mac,6);v.uReason=DOT11_ASSOC_STATUS_DISASSOCIATED_BY_OS;int e=a04_ndis_radio_reset(a);link_state(a,0);a04_ndis_status(a,NDIS_STATUS_DOT11_DISASSOCIATION,&v,sizeof(v));return e?NDIS_STATUS_FAILURE:NDIS_STATUS_SUCCESS;}
  if(oid==OID_DOT11_FLUSH_BSS_LIST){a->radio.bss_count=0;return NDIS_STATUS_SUCCESS;}
  if(oid==OID_DOT11_DESIRED_SSID_LIST){DOT11_SSID_LIST *v=(void *)p;ULONG base=FIELD_OFFSET(DOT11_SSID_LIST,SSIDs);if(n<base+sizeof(DOT11_SSID))return NDIS_STATUS_INVALID_LENGTH;if(v->Header.Type!=NDIS_OBJECT_TYPE_DEFAULT||v->Header.Revision!=1||v->uNumOfEntries!=1||v->uTotalNumOfEntries!=1||v->SSIDs[0].uSSIDLength>32)return NDIS_STATUS_INVALID_DATA;a->desired=v->SSIDs[0];}
  else if(oid==OID_DOT11_DESIRED_BSSID_LIST){DOT11_BSSID_LIST *v=(void *)p;if(n<FIELD_OFFSET(DOT11_BSSID_LIST,BSSIDs)+6||v->uNumOfEntries!=1)return NDIS_STATUS_INVALID_LENGTH;for(unsigned i=0;i<6;i++)if(v->BSSIDs[0][i]!=255)return NDIS_STATUS_NOT_SUPPORTED;}
  else if(oid==OID_DOT11_ENABLED_AUTHENTICATION_ALGORITHM||oid==OID_DOT11_ENABLED_UNICAST_CIPHER_ALGORITHM||oid==OID_DOT11_ENABLED_MULTICAST_CIPHER_ALGORITHM||oid==OID_DOT11_DESIRED_PHY_LIST){
   if(n<16)return NDIS_STATUS_INVALID_LENGTH;NdisMoveMemory(list,p,16);if(words[1]!=1||words[2]!=1)return NDIS_STATUS_INVALID_DATA;value=words[3];
   if(oid==OID_DOT11_DESIRED_PHY_LIST){if(value!=0&&value!=DOT11_PHY_ID_ANY)return NDIS_STATUS_NOT_SUPPORTED;}
   else{if(a->radio.connected)return NDIS_STATUS_DOT11_MEDIA_IN_USE;
    if(oid==OID_DOT11_ENABLED_AUTHENTICATION_ALGORITHM){if(value!=DOT11_AUTH_ALGO_80211_OPEN&&value!=DOT11_AUTH_ALGO_RSNA_PSK)return NDIS_STATUS_NOT_SUPPORTED;a->os_security.auth=value;}
    else{if(value!=DOT11_CIPHER_ALGO_NONE&&value!=DOT11_CIPHER_ALGO_CCMP)return NDIS_STATUS_NOT_SUPPORTED;if(oid==OID_DOT11_ENABLED_UNICAST_CIPHER_ALGORITHM)a->os_security.unicast=value;else a->os_security.multicast=value;}
   }
  }
  else if(oid==OID_DOT11_PRIVACY_EXEMPTION_LIST){
   DOT11_PRIVACY_EXEMPTION_LIST v;unsigned base=FIELD_OFFSET(DOT11_PRIVACY_EXEMPTION_LIST,PrivacyExemptionEntries);DOT11_PRIVACY_EXEMPTION entries[4];
   if(n<base)return NDIS_STATUS_INVALID_LENGTH;NdisZeroMemory(&v,sizeof(v));NdisMoveMemory(&v,p,base);
   if(v.Header.Type!=NDIS_OBJECT_TYPE_DEFAULT||v.Header.Revision!=1||v.uNumOfEntries>4||v.uTotalNumOfEntries<v.uNumOfEntries||v.uNumOfEntries>(n-base)/sizeof(entries[0]))return NDIS_STATUS_INVALID_DATA;
   NdisMoveMemory(entries,p+base,v.uNumOfEntries*sizeof(entries[0]));
   for(unsigned i=0;i<v.uNumOfEntries;i++)if(entries[i].usEtherType!=0x888e||(entries[i].usExemptionActionType!=1&&entries[i].usExemptionActionType!=2)||entries[i].usExemptionPacketType<1||entries[i].usExemptionPacketType>3)return NDIS_STATUS_NOT_SUPPORTED;
   a->os_security.exemption_count=v.uNumOfEntries;NdisMoveMemory(a->os_security.exemptions,entries,v.uNumOfEntries*sizeof(entries[0]));
  }
  else if(oid==OID_DOT11_DESIRED_COUNTRY_OR_REGION_STRING){if(n!=3)return NDIS_STATUS_INVALID_LENGTH;if(p[0]!=a->radio.bridge->regulatory.country[0]||p[1]!=a->radio.bridge->regulatory.country[1]||p[2]!=' ')return NDIS_STATUS_NOT_SUPPORTED;}
  else if(oid==OID_DOT11_NIC_POWER_STATE){if(n<1)return NDIS_STATUS_INVALID_LENGTH;if(!*p)return NDIS_STATUS_NOT_SUPPORTED;}
  else if(oid==OID_DOT11_EXCLUDE_UNENCRYPTED){if(n<1)return NDIS_STATUS_INVALID_LENGTH;a->os_security.exclude_unencrypted=!!*p;}
  else if(oid==OID_DOT11_CURRENT_OPERATION_MODE){DOT11_CURRENT_OPERATION_MODE *v=(void *)p;if(n<sizeof(*v)||v->uCurrentOpMode!=DOT11_OPERATION_MODE_EXTENSIBLE_STATION)return NDIS_STATUS_NOT_SUPPORTED;}
  else if(oid==OID_GEN_CURRENT_LOOKAHEAD||oid==OID_GEN_CURRENT_PACKET_FILTER||oid==OID_DOT11_DESIRED_BSS_TYPE||oid==OID_DOT11_CURRENT_PHY_ID||oid==OID_DOT11_AUTO_CONFIG_ENABLED){if(n<4)return NDIS_STATUS_INVALID_LENGTH;NdisMoveMemory(&value,p,4);if(oid==OID_GEN_CURRENT_LOOKAHEAD){if(value>1500)return NDIS_STATUS_INVALID_DATA;a->lookahead=value;}else if(oid==OID_GEN_CURRENT_PACKET_FILTER){if(value&~(NDIS_PACKET_TYPE_DIRECTED|NDIS_PACKET_TYPE_MULTICAST|NDIS_PACKET_TYPE_BROADCAST))return NDIS_STATUS_NOT_SUPPORTED;a->filter=value;}else if(oid==OID_DOT11_DESIRED_BSS_TYPE&&value!=dot11_BSS_type_infrastructure)return NDIS_STATUS_NOT_SUPPORTED;else if(oid==OID_DOT11_CURRENT_PHY_ID&&value!=0)return NDIS_STATUS_NOT_SUPPORTED;else if(oid==OID_DOT11_AUTO_CONFIG_ENABLED&&value!=3)return NDIS_STATUS_NOT_SUPPORTED;}
  else return NDIS_STATUS_NOT_SUPPORTED;
  q->DATA.SET_INFORMATION.BytesRead=n;return NDIS_STATUS_SUCCESS;
 }
 if(q->RequestType!=NdisRequestQueryInformation&&q->RequestType!=NdisRequestQueryStatistics)return NDIS_STATUS_NOT_SUPPORTED;
 switch(oid){
 case OID_GEN_SUPPORTED_LIST:return copy_query(q,oids,sizeof(oids));
 case OID_DOT11_CURRENT_ADDRESS:case OID_DOT11_PERMANENT_ADDRESS:return copy_query(q,a->radio.bridge->regulatory.mac,6);
 case OID_GEN_VENDOR_DESCRIPTION:{static const char name[]="MediaTek MT6630 Wireless LAN SDIO Adapter";return copy_query(q,name,sizeof(name));}
 case OID_GEN_VENDOR_ID:value=0x00ffffff;break;
 case OID_GEN_DRIVER_VERSION:{USHORT v=0x061e;return copy_query(q,&v,2);}
 case OID_GEN_HARDWARE_STATUS:value=NdisHardwareStatusReady;break;
 case OID_GEN_MEDIA_SUPPORTED:case OID_GEN_MEDIA_IN_USE:value=NdisMediumNative802_11;break;
 case OID_GEN_MAXIMUM_FRAME_SIZE:value=1500;break;case OID_GEN_MAXIMUM_TOTAL_SIZE:value=1532;break;
 case OID_GEN_LINK_SPEED:value=0xffffffffu;break;
 case OID_GEN_MEDIA_CONNECT_STATUS:value=a->radio.connected?NdisMediaStateConnected:NdisMediaStateDisconnected;break;
 case OID_GEN_CURRENT_LOOKAHEAD:value=a->lookahead;break;case OID_GEN_CURRENT_PACKET_FILTER:value=a->filter;break;
 case OID_DOT11_HARDWARE_PHY_STATE:case OID_DOT11_NIC_POWER_STATE:{BOOLEAN on=TRUE;return copy_query(q,&on,1);}
 case OID_DOT11_EXCLUDE_UNENCRYPTED:{BOOLEAN v=(BOOLEAN)a->os_security.exclude_unencrypted;return copy_query(q,&v,1);}
 case OID_DOT11_CIPHER_DEFAULT_KEY_ID:value=a->os_security.default_id;break;
 case OID_DOT11_DESIRED_BSS_TYPE:value=dot11_BSS_type_infrastructure;break;case OID_DOT11_AUTO_CONFIG_ENABLED:value=3;break;case OID_DOT11_CURRENT_PHY_ID:value=0;break;
 case OID_DOT11_CURRENT_CHANNEL:value=a->radio.connected?a->radio.station.ap.channel:0;break;
 case OID_DOT11_CURRENT_FREQUENCY:value=a->radio.connected?(2407+5*a->radio.station.ap.channel)*1000:0;break;
 case OID_DOT11_EXTSTA_CAPABILITY:{DOT11_EXTSTA_CAPABILITY v;NdisZeroMemory(&v,sizeof(v));HDR(v,DOT11_EXTSTA_CAPABILITY,1);v.uScanSSIDListSize=v.uDesiredBSSIDListSize=v.uDesiredSSIDListSize=1;v.uKeyMappingTableSize=1;v.uDefaultKeyTableSize=4;v.uPrivacyExemptionListSize=4;return copy_query(q,&v,sizeof(v));}
 case OID_DOT11_PRIVACY_EXEMPTION_LIST:{union {DOT11_PRIVACY_EXEMPTION_LIST align;UCHAR data[sizeof(DOT11_PRIVACY_EXEMPTION_LIST)+4*sizeof(DOT11_PRIVACY_EXEMPTION)];} v;DOT11_PRIVACY_EXEMPTION_LIST *head=&v.align;NdisZeroMemory(&v,sizeof(v));HDR(*head,DOT11_PRIVACY_EXEMPTION_LIST,1);head->uNumOfEntries=head->uTotalNumOfEntries=a->os_security.exemption_count;NdisMoveMemory(head->PrivacyExemptionEntries,a->os_security.exemptions,head->uNumOfEntries*sizeof(DOT11_PRIVACY_EXEMPTION));return copy_query(q,&v,FIELD_OFFSET(DOT11_PRIVACY_EXEMPTION_LIST,PrivacyExemptionEntries)+head->uNumOfEntries*sizeof(DOT11_PRIVACY_EXEMPTION));}
 case OID_DOT11_CURRENT_OPERATION_MODE:{DOT11_CURRENT_OPERATION_MODE v={0};v.uCurrentOpMode=DOT11_OPERATION_MODE_EXTENSIBLE_STATION;return copy_query(q,&v,sizeof(v));}
 case OID_DOT11_OPERATION_MODE_CAPABILITY:{DOT11_OPERATION_MODE_CAPABILITY v={0};v.uMajorVersion=2;v.uMinorVersion=0;v.uNumOfTXBuffers=v.uNumOfRXBuffers=1;v.uOpModeCapability=DOT11_OPERATION_MODE_EXTENSIBLE_STATION;return copy_query(q,&v,sizeof(v));}
 case OID_DOT11_SUPPORTED_PHY_TYPES:{DOT11_SUPPORTED_PHY_TYPES v={0};v.uNumOfEntries=v.uTotalNumOfEntries=1;v.dot11PHYType[0]=dot11_phy_type_erp;return copy_query(q,&v,sizeof(v));}
 case OID_DOT11_DESIRED_SSID_LIST:{DOT11_SSID_LIST v;NdisZeroMemory(&v,sizeof(v));HDR(v,DOT11_SSID_LIST,1);v.uNumOfEntries=v.uTotalNumOfEntries=1;v.SSIDs[0]=a->desired;return copy_query(q,&v,sizeof(v));}
 case OID_DOT11_DESIRED_BSSID_LIST:{DOT11_BSSID_LIST v;NdisZeroMemory(&v,sizeof(v));HDR(v,DOT11_BSSID_LIST,1);v.uNumOfEntries=v.uTotalNumOfEntries=1;NdisFillMemory(v.BSSIDs[0],6,255);return copy_query(q,&v,sizeof(v));}
 case OID_DOT11_ENABLED_AUTHENTICATION_ALGORITHM:case OID_DOT11_ENABLED_UNICAST_CIPHER_ALGORITHM:case OID_DOT11_ENABLED_MULTICAST_CIPHER_ALGORITHM:case OID_DOT11_DESIRED_PHY_LIST:{NDIS_OBJECT_HEADER *h=(void *)list;NdisZeroMemory(list,16);h->Type=NDIS_OBJECT_TYPE_DEFAULT;h->Revision=1;h->Size=16;words[1]=words[2]=1;words[3]=oid==OID_DOT11_ENABLED_AUTHENTICATION_ALGORITHM?(a->os_security.auth?a->os_security.auth:DOT11_AUTH_ALGO_80211_OPEN):(oid==OID_DOT11_ENABLED_UNICAST_CIPHER_ALGORITHM?a->os_security.unicast:(oid==OID_DOT11_ENABLED_MULTICAST_CIPHER_ALGORITHM?a->os_security.multicast:0));return copy_query(q,list,16);}
 case OID_DOT11_SUPPORTED_UNICAST_ALGORITHM_PAIR:case OID_DOT11_SUPPORTED_MULTICAST_ALGORITHM_PAIR:{union {DOT11_AUTH_CIPHER_PAIR_LIST align;UCHAR data[sizeof(DOT11_AUTH_CIPHER_PAIR_LIST)+sizeof(DOT11_AUTH_CIPHER_PAIR)];} v;DOT11_AUTH_CIPHER_PAIR_LIST *head=&v.align;NdisZeroMemory(&v,sizeof(v));HDR(*head,DOT11_AUTH_CIPHER_PAIR_LIST,1);head->uNumOfEntries=head->uTotalNumOfEntries=2;head->AuthCipherPairs[0].AuthAlgoId=DOT11_AUTH_ALGO_80211_OPEN;head->AuthCipherPairs[0].CipherAlgoId=DOT11_CIPHER_ALGO_NONE;head->AuthCipherPairs[1].AuthAlgoId=DOT11_AUTH_ALGO_RSNA_PSK;head->AuthCipherPairs[1].CipherAlgoId=DOT11_CIPHER_ALGO_CCMP;return copy_query(q,&v,FIELD_OFFSET(DOT11_AUTH_CIPHER_PAIR_LIST,AuthCipherPairs)+2*sizeof(DOT11_AUTH_CIPHER_PAIR));}
 case OID_DOT11_SUPPORTED_COUNTRY_OR_REGION_STRING:{DOT11_COUNTRY_OR_REGION_STRING_LIST v;NdisZeroMemory(&v,sizeof(v));HDR(v,DOT11_COUNTRY_OR_REGION_STRING_LIST,1);v.uNumOfEntries=v.uTotalNumOfEntries=1;v.CountryOrRegionStrings[0][0]=a->radio.bridge->regulatory.country[0];v.CountryOrRegionStrings[0][1]=a->radio.bridge->regulatory.country[1];v.CountryOrRegionStrings[0][2]=' ';return copy_query(q,&v,sizeof(v));}
 case OID_DOT11_DESIRED_COUNTRY_OR_REGION_STRING:{UCHAR country[3]={a->radio.bridge->regulatory.country[0],a->radio.bridge->regulatory.country[1],' '};return copy_query(q,country,3);}
 default:return NDIS_STATUS_NOT_SUPPORTED;
 }
 return copy_query(q,&value,sizeof(value));
}
