/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "../drivers/network/mt6630/station_selection.h"
#include "../drivers/network/mt6630/wmt_bridge.c"
struct fake {unsigned requests;uint32_t status;};
static int direct(void *context,uint32_t arg,uint32_t *out){(void)context;(void)arg;*out=0;return 0;}
static int transfer_fake(void *context,uint32_t arg,unsigned write,uint8_t *data,size_t n)
{
 struct fake *f=context;unsigned address=(arg>>9)&0x1ffffu;uint32_t v;
 assert(!write && n==4 && ((arg>>28)&7)==1);f->requests++;
 assert(address==0x10 || address==0x20);v=address==0x10?f->status:0;
 data[0]=(uint8_t)v;data[1]=(uint8_t)(v>>8);data[2]=(uint8_t)(v>>16);data[3]=(uint8_t)(v>>24);return 0;
}
static int done(void *context){(void)context;return 0;}
static void candidate_selection_regression(void)
{
 struct a04_wlan_local local={0};struct a04_wlan_ap ap[3]={0};
 const uint8_t synthetic_ssid[1]={1};unsigned length,i,chosen;
 const uint8_t *rsn=a04_wlan_rsn_selected(&length);
 local.active_channels=local.permitted_channels=1u<<1;
 for(i=0;i<3;i++){
  ap[i].ssid_length=1;ap[i].ssid[0]=1;ap[i].channel=1;ap[i].capability=16;
  ap[i].rsn_length=length;for(unsigned j=0;j<length;j++)ap[i].rsn[j]=rsn[j];
 }
 ap[0].capability=0;ap[0].rsn_length=0; /* Earlier OPEN BSS. */
 ap[1].rsn[7]=2; /* Earlier incompatible TKIP group suite. */
 for(chosen=0;chosen<3;chosen++)if(a04_station_candidate_matches(&local,&ap[chosen],synthetic_ssid,1,1))break;
 assert(chosen==2); /* Compatible CCMP BSS must still be reachable. */
 assert(a04_station_candidate_matches(&local,&ap[0],synthetic_ssid,1,0));
 assert(!a04_station_candidate_matches(&local,&ap[2],synthetic_ssid,1,0));
 ap[2].channel=6;assert(!a04_station_candidate_matches(&local,&ap[2],synthetic_ssid,1,1));
}
int main(void)
{
 candidate_selection_regression();
 uint8_t cis[]={0x20,4,0x7a,0x03,0x30,0x66,0xff};uint32_t status=0;
 struct a04_wifi_owner owner={0,1,1,1,1,1,0,0};struct a04_wifi_life life={0};struct fake f={0};
 struct a04_sdio_direct d={&owner,&f,direct,0,0};struct a04_sdio_bus bus={&f,transfer_fake};
 struct a04_wifi_life_ops ops={&f,done,done,done,done};struct a04_wmt_bridge b={0};
 assert(!a04_wifi_admit(&owner));assert(!a04_wifi_life_begin(&life,&owner,cis,sizeof(cis),&d,&bus,&ops));
 b.life=&life;b.wlan_ready=1;b.io.ctx=&b;b.io.command=command;b.io.delay=delay_us;
 b.transport.io=&b.io;b.transport.transfer=transfer;b.transport.enabled=2;b.transport.functions=2;
 b.wlan.transport=&b.transport;f.status=3;
 assert(!a04_wmt_bridge_poll_wlan_status(&b,&status)&&status==3&&f.requests==1);
 assert(a04_wmt_bridge_poll_wlan_status(&b,&status)<0&&f.requests==1);
 assert(a04_wmt_bridge_ack_wlan_status(&b,1)<0);assert(!a04_wmt_bridge_ack_wlan_status(&b,3));
 {uint8_t event[12]={12,0,0,0xe0,0x0d,0,0,0,7,0,0,0};
  b.scan.pending=1;b.scan.sequence=7;
  assert(!a04_wmt_bridge_scan_event(&b,event,sizeof(event)) && b.scan.complete);
  assert(!a04_wmt_bridge_scan_consume(&b) && !b.scan.complete);
 }
 f.status=0x40;assert(a04_wmt_bridge_poll_wlan_status(&b,&status)!=0);
 assert(b.failed && owner.poisoned && life.state==A04_WIFI_LIFE_FAILED && f.requests==3);
 puts("WLAN read-clear status accounting and poison: OK");return 0;
}
