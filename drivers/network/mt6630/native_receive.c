/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/windot11/ns-windot11-dot11_extsta_recv_context
 */
#include "native_receive.h"
NDIS_STATUS a04_native_indicate_mpdu(NDIS_HANDLE adapter,PNET_BUFFER_LIST nbl,
 const uint8_t *mpdu,ULONG length,const struct a04_native_rx_info *info)
{
 DOT11_EXTSTA_RECV_CONTEXT receive;PNET_BUFFER nb;PVOID dst,old,old_priority;NDIS_NET_BUFFER_LIST_8021Q_INFO priority;
 if(!adapter||!nbl||!mpdu||!info||length<24||length>4096||
    info->priority>7||info->observed!=A04_RX_OBS_REQUIRED||!info->frequency||!info->fragments||
    info->fragments>DOT11_MAX_NUM_OF_FRAGMENTS||NET_BUFFER_LIST_NEXT_NBL(nbl))return NDIS_STATUS_INVALID_DATA;
 nb=NET_BUFFER_LIST_FIRST_NB(nbl);
 if(!nb||NET_BUFFER_NEXT_NB(nb)||NET_BUFFER_DATA_LENGTH(nb)!=length)return NDIS_STATUS_INVALID_LENGTH;
 dst=NdisGetDataBuffer(nb,length,NULL,1,0);if(!dst)return NDIS_STATUS_RESOURCES;
 if(dst!=mpdu)NdisMoveMemory(dst,mpdu,length);
 NdisZeroMemory(&receive,sizeof(receive));
 receive.Header.Type=NDIS_OBJECT_TYPE_DEFAULT;receive.Header.Revision=DOT11_EXTSTA_RECV_CONTEXT_REVISION_1;
 receive.Header.Size=sizeof(receive);receive.uPhyId=info->phy;receive.uChCenterFrequency=info->frequency;
 receive.usNumberOfMPDUsReceived=info->fragments;receive.lRSSI=info->rssi;receive.ucDataRate=info->rate;
 old=NET_BUFFER_LIST_INFO(nbl,MediaSpecificInformation);
 NET_BUFFER_LIST_INFO(nbl,MediaSpecificInformation)=&receive;
 old_priority=NET_BUFFER_LIST_INFO(nbl,Ieee8021QNetBufferListInfo);priority.Value=0;priority.TagHeader.UserPriority=info->priority;
 NET_BUFFER_LIST_INFO(nbl,Ieee8021QNetBufferListInfo)=priority.Value;
 NdisMIndicateReceiveNetBufferLists(adapter,nbl,0,1,NDIS_RECEIVE_FLAGS_RESOURCES);
 NET_BUFFER_LIST_INFO(nbl,MediaSpecificInformation)=old;
 NET_BUFFER_LIST_INFO(nbl,Ieee8021QNetBufferListInfo)=old_priority;
 return NDIS_STATUS_SUCCESS;
}
