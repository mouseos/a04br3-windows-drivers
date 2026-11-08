/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/network/mt6630/wmt_bridge.c"
#include "native_nbl.h"
#include "native_tx.h"
#include "native_limits.h"
struct fake {unsigned sends,completed;NDIS_STATUS result;};static struct fake *active;
void *NdisGetDataBuffer(PNET_BUFFER nb,ULONG n,void *storage,ULONG align,ULONG offset)
{assert(n==nb->length && align==1 && !offset);if(nb->fragmented){memcpy(storage,nb->data,n);return storage;}return nb->data;}
void NdisMSendNetBufferListsComplete(NDIS_HANDLE adapter,PNET_BUFFER_LIST nbl,ULONG flags)
{assert(adapter==active && !flags);active->completed++;active->result=nbl->status;}
static unsigned hook_calls;
static void completion_hook(void *context,PNET_BUFFER_LIST nbl,NDIS_STATUS result)
{assert(context==active);hook_calls++;NET_BUFFER_LIST_STATUS(nbl)=result;NdisMSendNetBufferListsComplete(context,nbl,0);}
static int direct(void *c,uint32_t a,uint32_t *r){(void)c;(void)a;*r=0;return 0;}
static int send(void *context,uint32_t arg,unsigned write,uint8_t *data,size_t n)
{struct fake *f=context;assert(write && ((arg>>9)&0x1ffff)==0x34 && n>=42);assert(data[28]==2);f->sends++;return 0;}
static int done(void *c){(void)c;return 0;}
int main(void)
{
 uint8_t cis[]={0x20,4,0x7a,3,0x30,0x66,0xff},mac[]={2,1,2,3,4,5},mpdu[32]={0},snapshot[64],eth[64],event[24]={24,0,0,0xe0,0x0f};
 struct a04_wifi_owner o={0,1,1,1,1,1,0,0};struct a04_wifi_life life={0};struct fake f={0};
 struct a04_sdio_direct d={&o,&f,direct,0,0};struct a04_sdio_bus bus={&f,send};struct a04_wifi_life_ops ops={&f,done,done,done,done};
 struct a04_wmt_bridge b={0};struct a04_wlan_station sta={0};struct a04_wlan_security sec={0};struct a04_native_tx tx={0};
 NET_BUFFER nb={0,sizeof(mpdu),mpdu,1};NET_BUFFER_LIST nbl={0,&nb,0,0,0};
 assert(!a04_wifi_admit(&o));assert(!a04_wifi_life_begin(&life,&o,cis,sizeof(cis),&d,&bus,&ops));
 b.life=&life;b.wlan_ready=1;b.io.ctx=&b;b.io.command=command;b.io.delay=delay_us;b.transport.io=&b.io;b.transport.transfer=transfer;b.transport.enabled=2;b.transport.functions=2;
 b.wlan.transport=&b.transport;b.wlan.initialized=1;b.wlan.runtime=1;b.wlan.tc0_free=13;sta.sta_active=1;sta.join.phase=4;sta.local.wlan=3;
 memcpy(sta.local.mac,mac,6);memcpy(sta.ap.mac,"\x02\x11\x22\x33\x44\x55",6);
 mpdu[0]=8;mpdu[1]=1;memcpy(mpdu+4,sta.ap.mac,6);memcpy(mpdu+10,mac,6);mpdu[16]=2;memcpy(mpdu+24,"\xaa\xaa\x03\x00\x00\x00\x08\x00",8);
 active=&f;tx.adapter=&f;tx.completion_context=&f;tx.completion=completion_hook;
 assert(a04_native_nbl_send(&tx,&b,&sta,&sec,&nbl,snapshot,sizeof(snapshot),eth,sizeof(eth),NULL,NULL,10,100)==NDIS_STATUS_PENDING);
 assert(f.sends==1 && !f.completed && tx.pending==&nbl);
 event[8]=(uint8_t)tx.state.pid;event[12]=3;
 assert(a04_native_nbl_tx_event(&tx,&b,&sta,&sec,event,sizeof(event),20)==NDIS_STATUS_SUCCESS);
 assert(hook_calls==1 && f.completed==1 && f.result==NDIS_STATUS_SUCCESS && !tx.pending);
 assert(a04_native_nbl_tx_event(&tx,&b,&sta,&sec,event,sizeof(event),20)==NDIS_STATUS_INVALID_DATA && f.completed==1);

 uint8_t qos[38],original[38];memcpy(qos,mpdu,24);qos[0]=0x88;qos[1]=0x81;qos[24]=6;qos[25]=0;memset(qos+26,0,4);memcpy(qos+30,mpdu+24,8);memcpy(original,qos,38);
 nb.length=38;nb.data=qos;assert(a04_native_nbl_send(&tx,&b,&sta,&sec,&nbl,snapshot,sizeof(snapshot),eth,sizeof(eth),NULL,NULL,25,100)==NDIS_STATUS_PENDING);
 assert((b.wlan.tx[6]&0x70)==0x60&&!memcmp(original,qos,38));event[8]=(uint8_t)tx.state.pid;
 assert(!a04_native_nbl_tx_event(&tx,&b,&sta,&sec,event,sizeof(event),26));
 NDIS_NET_BUFFER_LIST_8021Q_INFO q;q.Value=0;q.TagHeader.UserPriority=3;nbl.priority=q.Value;nb.length=32;nb.data=mpdu;
 assert(a04_native_nbl_send(&tx,&b,&sta,&sec,&nbl,snapshot,sizeof(snapshot),eth,sizeof(eth),NULL,NULL,27,100)==NDIS_STATUS_PENDING);assert((b.wlan.tx[6]&0x70)==0x30);
 event[8]=(uint8_t)tx.state.pid;assert(!a04_native_nbl_tx_event(&tx,&b,&sta,&sec,event,sizeof(event),28));nbl.priority=0;
 /* Full Native MSDU uses independent TC1/BE resources. */
 uint8_t full[A04_NATIVE_MTU+32],converted[A04_NATIVE_ETH_MAX];size_t written=0;
 memset(full,0,sizeof(full));memcpy(full,mpdu,sizeof(mpdu));
 assert(!a04_native_tx_to_ethernet(full,sizeof(full),sta.ap.mac,mac,converted,sizeof(converted),&written));
 assert(written==A04_NATIVE_ETH_MAX);
 assert(a04_native_tx_to_ethernet(full,sizeof(full),sta.ap.mac,mac,converted,sizeof(converted)-1,&written)&&!written);
 assert((A04_NATIVE_ETH_MAX+28+127)/128>13&&A04_RUNTIME_MTU==2304);
 assert(hook_calls==3);
 assert(!a04_wlan_credit_return(&b.wlan,3,0,0,3));
 assert(!a04_wifi_life_block_verified(&life,1,512));
 assert(!a04_wlan_native_be_enable(&b.wlan)&&b.wlan.tc1_free==468);
 nb.length=sizeof(full);nb.data=full;
 assert(a04_native_nbl_send(&tx,&b,&sta,&sec,&nbl,full,sizeof(full),converted,sizeof(converted),NULL,NULL,40,100)==NDIS_STATUS_PENDING);
 assert(b.wlan.tx[3]==8&&b.wlan.tc1_used==19&&b.wlan.tc1_free==449);
 event[8]=(uint8_t)tx.state.pid;
 assert(a04_native_nbl_tx_event(&tx,&b,&sta,&sec,event,sizeof(event),50)==NDIS_STATUS_SUCCESS);
 assert(b.wlan.tc1_used==19&&b.wlan.tc1_free==449); /* TX_DONE is not credit */
 assert(!a04_wlan_credit_return(&b.wlan,0,19,0,0)&&b.wlan.tc1_free==449);
 assert(!a04_wlan_credit_return(&b.wlan,0,0,0,19)&&b.wlan.tc1_free==468&&!b.wlan.tc1_used);
 assert(a04_native_nbl_send(&tx,&b,&sta,&sec,&nbl,full,sizeof(full),converted,sizeof(converted),NULL,NULL,60,100)==NDIS_STATUS_PENDING);
 assert(!a04_native_nbl_abort(&tx,&b)&&f.completed==5&&f.result==NDIS_STATUS_REQUEST_ABORTED&&o.poisoned);
 assert(hook_calls==5);
 assert(a04_wlan_credit_return(&b.wlan,0,20,0,20)==A04_WLAN_CREDIT);
 puts("NDIS NBL copy, firmware completion and abort: OK");return 0;
}
