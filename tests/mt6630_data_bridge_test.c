/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/network/mt6630/wmt_bridge.c"
struct fake {unsigned sends;uint8_t bytes[128];};
static int direct(void *c,uint32_t a,uint32_t *r){(void)c;(void)a;*r=0;return 0;}
static int send(void *context,uint32_t arg,unsigned write,uint8_t *data,size_t n)
{struct fake *f=context;assert(write && ((arg>>28)&7)==1 && ((arg>>9)&0x1ffff)==0x34 && n<=sizeof(f->bytes));memcpy(f->bytes,data,n);f->sends++;return 0;}
static int done(void *c){(void)c;return 0;}
int main(void)
{
 uint8_t cis[]={0x20,4,0x7a,3,0x30,0x66,0xff},mac[]={2,1,2,3,4,5};
 struct a04_wifi_owner o={0,1,1,1,1,1,0,0};struct a04_wifi_life life={0};struct fake f={0};
 struct a04_sdio_direct d={&o,&f,direct,0,0};struct a04_sdio_bus bus={&f,send};struct a04_wifi_life_ops ops={&f,done,done,done,done};
 struct a04_wmt_bridge b={0};struct a04_wlan_station sta={0};struct a04_wlan_security sec={0};struct a04_wlan_data_state state={0};struct a04_wlan_ethernet view;
 uint8_t frame[14]={0},rx[30]={0},event[24]={24,0,0,0xe0,0x0f};
 assert(!a04_wifi_admit(&o));assert(!a04_wifi_life_begin(&life,&o,cis,sizeof(cis),&d,&bus,&ops));
 b.life=&life;b.wlan_ready=1;b.io.ctx=&b;b.io.command=command;b.io.delay=delay_us;b.transport.io=&b.io;b.transport.transfer=transfer;b.transport.enabled=2;b.transport.functions=2;
 b.wlan.transport=&b.transport;b.wlan.initialized=1;b.wlan.runtime=1;b.wlan.tc0_free=13;sta.sta_active=1;sta.join.phase=4;sta.local.wlan=3;memcpy(sta.local.mac,mac,6);
 frame[0]=2;memcpy(frame+6,mac,6);frame[12]=8;
 assert(!a04_wmt_bridge_tx_ethernet(&b,&sta,&sec,&state,frame,sizeof(frame),NULL,NULL,10,100));
 assert(f.sends==1 && state.pending && !memcmp(f.bytes+28,frame,sizeof(frame)));
 event[8]=(uint8_t)state.pid;event[12]=3;
 assert(!a04_wmt_bridge_tx_event(&b,&sta,&sec,&state,event,sizeof(event),20) && !state.pending && state.confirmed==1);
 rx[0]=30;rx[3]=0x40;rx[6]=0x8e;rx[8]=3;memcpy(rx+16,mac,6);rx[28]=8;
 assert(!a04_wmt_bridge_rx_ethernet(&b,&sta,&sec,rx,sizeof(rx),&view) && view.length==14 && view.data==rx+16);
 sta.sta_active=0;assert(a04_wmt_bridge_rx_ethernet(&b,&sta,&sec,rx,sizeof(rx),&view)==A04_WLAN_SECURITY);
 puts("WLAN data packet and completion fixture: OK");return 0;
}
