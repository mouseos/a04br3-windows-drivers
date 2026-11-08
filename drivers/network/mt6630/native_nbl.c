/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "native_nbl.h"
#include "native_tx.h"
#include "native_frame.h"
#include "native_limits.h"
static void complete(struct a04_native_tx *tx,NDIS_STATUS result)
{
 PNET_BUFFER_LIST nbl=tx->pending;tx->pending=NULL;
 NET_BUFFER_LIST_STATUS(nbl)=result;
 if(tx->completion)tx->completion(tx->completion_context,nbl,result);
 else NdisMSendNetBufferListsComplete(tx->adapter,nbl,0);
}
NDIS_STATUS a04_native_nbl_send(struct a04_native_tx *tx,struct a04_wmt_bridge *b,
 const struct a04_wlan_station *station,struct a04_wlan_security *security,PNET_BUFFER_LIST nbl,
 uint8_t *mpdu,size_t mpdu_capacity,uint8_t *ethernet,size_t ethernet_capacity,
 a04_wlan_mic_verify verify,void *context,uint64_t now,uint64_t deadline)
{
 PNET_BUFFER nb;PVOID view;ULONG length;size_t converted;int e;struct a04_native_frame frame;
 if(!tx||!tx->adapter||tx->pending||!b||!station||!security||!nbl||!mpdu||!ethernet)return NDIS_STATUS_INVALID_DATA;
 if(NET_BUFFER_LIST_NEXT_NBL(nbl))return NDIS_STATUS_NOT_SUPPORTED;
 nb=NET_BUFFER_LIST_FIRST_NB(nbl);if(!nb||NET_BUFFER_NEXT_NB(nb))return NDIS_STATUS_NOT_SUPPORTED;
 length=NET_BUFFER_DATA_LENGTH(nb);
 if(length<32||length>A04_NATIVE_MPDU_MAX||length>mpdu_capacity)return NDIS_STATUS_INVALID_LENGTH;
 view=NdisGetDataBuffer(nb,length,mpdu,1,0);if(!view)return NDIS_STATUS_RESOURCES;
 if(view!=mpdu)NdisMoveMemory(mpdu,view,length);
 if(a04_native_tx_to_ethernet(mpdu,length,station->ap.mac,station->local.mac,ethernet,ethernet_capacity,&converted))return NDIS_STATUS_INVALID_DATA;
 if(a04_native_frame_parse(mpdu,length,1,&frame))return NDIS_STATUS_INVALID_DATA;
 NDIS_NET_BUFFER_LIST_8021Q_INFO priority;priority.Value=NET_BUFFER_LIST_INFO(nbl,Ieee8021QNetBufferListInfo);
 if(frame.qos&&priority.TagHeader.UserPriority&&priority.TagHeader.UserPriority!=frame.tid)return NDIS_STATUS_INVALID_DATA;
 tx->state.tid=frame.qos?frame.tid:(unsigned)priority.TagHeader.UserPriority;
 e=a04_wmt_bridge_tx_ethernet(b,station,security,&tx->state,ethernet,(unsigned)converted,verify,context,now,deadline);
 if(e)return NDIS_STATUS_FAILURE;
 tx->pending=nbl;return NDIS_STATUS_PENDING;
}
NDIS_STATUS a04_native_nbl_tx_event(struct a04_native_tx *tx,struct a04_wmt_bridge *b,
 const struct a04_wlan_station *station,struct a04_wlan_security *security,const uint8_t *packet,unsigned length,uint64_t now)
{
 int e;if(!tx||!tx->adapter||!tx->pending||!b||!station||!security||!packet)return NDIS_STATUS_INVALID_DATA;
 e=a04_wmt_bridge_tx_event(b,station,security,&tx->state,packet,length,now);
 if(e==A04_WLAN_UNSUPPORTED)return NDIS_STATUS_NOT_SUPPORTED;
 if(e){
  security->authorized=0;b->failed=1;b->wlan_ready=0;
  if(b->life && b->life->state==A04_WIFI_LIFE_IO){b->life->state=A04_WIFI_LIFE_FAILED;b->life->transport.ready=0;
   if(b->life->owner&&!b->life->owner->poisoned)a04_wifi_fault(b->life->owner);}
  complete(tx,NDIS_STATUS_FAILURE);return NDIS_STATUS_FAILURE;
 }
 if(tx->state.pending)return NDIS_STATUS_PENDING;
 complete(tx,NDIS_STATUS_SUCCESS);return NDIS_STATUS_SUCCESS;
}
NDIS_STATUS a04_native_nbl_abort(struct a04_native_tx *tx,struct a04_wmt_bridge *b)
{
 if(!tx||!tx->adapter||!tx->pending||!b||!b->life)return NDIS_STATUS_INVALID_DATA;
 /* An unknown in-flight firmware completion cannot be reused in this epoch. */
 tx->state.failed=A04_WLAN_STATE;tx->state.pending=0;b->failed=1;b->wlan_ready=0;
 if(b->life->state==A04_WIFI_LIFE_IO){b->life->state=A04_WIFI_LIFE_FAILED;b->life->transport.ready=0;
  if(b->life->owner&&!b->life->owner->poisoned)a04_wifi_fault(b->life->owner);}
 complete(tx,NDIS_STATUS_REQUEST_ABORTED);return NDIS_STATUS_SUCCESS;
}
