/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/network/mt6630/wmt_bridge.c"
#include "native_nbl.h"
struct fake {unsigned sends,completed;NDIS_STATUS result;};static struct fake *active;
void *NdisGetDataBuffer(PNET_BUFFER nb,ULONG n,void *storage,ULONG align,ULONG offset)
{assert(n==nb->length && align==1 && !offset);if(nb->fragmented){memcpy(storage,nb->data,n);return storage;}return nb->data;}
void NdisMSendNetBufferListsComplete(NDIS_HANDLE adapter,PNET_BUFFER_LIST nbl,ULONG flags)
{assert(adapter==active && !flags);active->completed++;active->result=nbl->status;}
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
 NET_BUFFER nb={0,sizeof(mpdu),mpdu,1};NET_BUFFER_LIST nbl={0,&nb,0,0};
 assert(!a04_wifi_admit(&o));assert(!a04_wifi_life_begin(&life,&o,cis,sizeof(cis),&d,&bus,&ops));
 b.life=&life;b.wlan_ready=1;b.io.ctx=&b;b.io.command=command;b.io.delay=delay_us;b.transport.io=&b.io;b.transport.transfer=transfer;b.transport.enabled=2;b.transport.functions=2;
 b.wlan.transport=&b.transport;b.wlan.initialized=1;b.wlan.runtime=1;b.wlan.tc0_free=13;sta.sta_active=1;sta.join.phase=4;sta.local.wlan=3;
 memcpy(sta.local.mac,mac,6);memcpy(sta.ap.mac,"\x02\x11\x22\x33\x44\x55",6);
 mpdu[0]=8;mpdu[1]=1;memcpy(mpdu+4,sta.ap.mac,6);memcpy(mpdu+10,mac,6);mpdu[16]=2;memcpy(mpdu+24,"\xaa\xaa\x03\x00\x00\x00\x08\x00",8);
 active=&f;tx.adapter=&f;
 assert(a04_native_nbl_send(&tx,&b,&sta,&sec,&nbl,snapshot,sizeof(snapshot),eth,sizeof(eth),NULL,NULL,10,100)==NDIS_STATUS_PENDING);
 assert(f.sends==1 && !f.completed && tx.pending==&nbl);
 event[8]=(uint8_t)tx.state.pid;event[12]=3;
 assert(a04_native_nbl_tx_event(&tx,&b,&sta,&sec,event,sizeof(event),20)==NDIS_STATUS_SUCCESS);
 assert(f.completed==1 && f.result==NDIS_STATUS_SUCCESS && !tx.pending);
 assert(a04_native_nbl_tx_event(&tx,&b,&sta,&sec,event,sizeof(event),20)==NDIS_STATUS_INVALID_DATA && f.completed==1);
 assert(a04_native_nbl_send(&tx,&b,&sta,&sec,&nbl,snapshot,sizeof(snapshot),eth,sizeof(eth),NULL,NULL,30,100)==NDIS_STATUS_PENDING);
 assert(!a04_native_nbl_abort(&tx,&b) && f.completed==2 && f.result==NDIS_STATUS_REQUEST_ABORTED && o.poisoned);
 puts("NDIS NBL copy, firmware completion and abort: OK");return 0;
}
