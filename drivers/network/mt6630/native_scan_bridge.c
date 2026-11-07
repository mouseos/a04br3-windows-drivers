/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * https://learn.microsoft.com/en-us/windows-hardware/drivers/network/oid-dot11-scan-request
 * https://learn.microsoft.com/en-us/previous-versions/windows/hardware/wireless/ndis-status-dot11-scan-confirm
 */
#include "native_scan_bridge.h"
NDIS_STATUS a04_native_scan_start(struct a04_wmt_bridge *b,const void *input,ULONG bytes,
 const struct a04_native_scan_policy *policy,ULONG *needed)
{
 struct a04_native_scan_request request;struct a04_wlan_scan scan={0};NDIS_STATUS status;int e;
 if(!b||!policy||!needed)return NDIS_STATUS_INVALID_DATA;
 status=a04_native_scan_parse(input,bytes,&request,needed);if(status!=NDIS_STATUS_SUCCESS)return status;
 if(b->scan.pending)return NDIS_STATUS_DOT11_MEDIA_IN_USE;
 if(!policy->channel_count || policy->channel_count>54)return NDIS_STATUS_INVALID_DATA;
 scan.sequence=policy->sequence;scan.bss=policy->bss;
 scan.active=request.scan_type==dot11_scan_type_active;
 scan.probes=policy->probes;scan.min_dwell=policy->min_dwell;scan.dwell=policy->dwell;scan.timeout=policy->timeout;
 scan.ssid=request.ssid;scan.ssid_length=request.ssid_length;scan.ie=request.ie;scan.ie_length=request.ie_length;
 scan.channels=policy->channels;scan.channel_count=policy->channel_count;scan.channel_type=4;scan.wire_version=3;
 e=a04_wmt_bridge_scan_begin(b,&scan);
 return e?NDIS_STATUS_FAILURE:NDIS_STATUS_SUCCESS;
}
static void indicate(NDIS_HANDLE adapter,NDIS_STATUS result)
{
 NDIS_STATUS_INDICATION indication;
 NdisZeroMemory(&indication,sizeof(indication));
 indication.Header.Type=NDIS_OBJECT_TYPE_STATUS_INDICATION;
 indication.Header.Revision=NDIS_STATUS_INDICATION_REVISION_1;
 indication.Header.Size=NDIS_SIZEOF_STATUS_INDICATION_REVISION_1;
 indication.SourceHandle=adapter;indication.StatusCode=NDIS_STATUS_DOT11_SCAN_CONFIRM;
 indication.StatusBuffer=&result;indication.StatusBufferSize=sizeof(result);
 NdisMIndicateStatusEx(adapter,&indication);
}
NDIS_STATUS a04_native_scan_indicate(struct a04_wmt_bridge *b,NDIS_HANDLE adapter)
{
 if(!b||!adapter || a04_wmt_bridge_scan_consume(b))return NDIS_STATUS_INVALID_DATA;
 indicate(adapter,NDIS_STATUS_SUCCESS);return NDIS_STATUS_SUCCESS;
}
NDIS_STATUS a04_native_scan_fail(struct a04_wmt_bridge *b,NDIS_HANDLE adapter,NDIS_STATUS result)
{
 if(!b||!adapter||result==NDIS_STATUS_SUCCESS||a04_wmt_bridge_scan_fail(b,A04_WLAN_STATE))return NDIS_STATUS_INVALID_DATA;
 indicate(adapter,result);return NDIS_STATUS_SUCCESS;
}
