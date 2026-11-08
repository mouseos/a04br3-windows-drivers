/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "native_receive.h"
static unsigned calls;static int marker;
void *NdisGetDataBuffer(PNET_BUFFER nb,ULONG n,void *storage,ULONG align,ULONG offset)
{assert(n==nb->length && !storage && align==1 && !offset);return nb->fragmented?NULL:nb->data;}
void NdisMIndicateReceiveNetBufferLists(NDIS_HANDLE a,PNET_BUFFER_LIST n,ULONG port,ULONG count,ULONG flags)
{
 DOT11_EXTSTA_RECV_CONTEXT *r=n->info;assert(a==&marker && !port && count==1 && flags==NDIS_RECEIVE_FLAGS_RESOURCES);
 assert(r && r->Header.Revision==1 && !r->uReceiveFlags && r->uPhyId==2 && r->uChCenterFrequency==2437 && r->lRSSI==-48 && r->ucDataRate==4);NDIS_NET_BUFFER_LIST_8021Q_INFO q;q.Value=n->priority;assert(q.TagHeader.UserPriority==5);calls++;
}
int main(void)
{
 uint8_t mpdu[30]={8},dest[30]={0};NET_BUFFER nb={0,sizeof(dest),dest,0};NET_BUFFER_LIST nbl={0,&nb,0,&marker,&marker};
 struct a04_native_rx_info info={A04_RX_OBS_REQUIRED,2,2437,1,-48,4,5};
 assert(a04_native_indicate_mpdu(&marker,&nbl,mpdu,sizeof(mpdu),&info)==NDIS_STATUS_SUCCESS && calls==1);
 assert(!memcmp(dest,mpdu,sizeof(mpdu)) && nbl.info==&marker && nbl.priority==&marker);
 info.observed&=~A04_RX_OBS_RSSI;assert(a04_native_indicate_mpdu(&marker,&nbl,mpdu,sizeof(mpdu),&info)==NDIS_STATUS_INVALID_DATA && calls==1);
 info.observed=A04_RX_OBS_REQUIRED;nb.fragmented=1;assert(a04_native_indicate_mpdu(&marker,&nbl,mpdu,sizeof(mpdu),&info)==NDIS_STATUS_RESOURCES && calls==1);
 puts("NDIS MPDU OOB indication and buffer lifetime: OK");return 0;
}
