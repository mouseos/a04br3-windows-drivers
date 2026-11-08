/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Actual runtime packet -> bridge scan completion -> Native confirmation ->
 * station selector. No RF success, credentials or NDIS data ACK fabricated. */
#include <assert.h>
#include <string.h>
#include "../drivers/network/mt6630/wmt_bridge.c"
#include "../drivers/network/mt6630/station_runtime.c"
#include "native_scan_bridge.h"
static unsigned sends,confirmed;
int KeGetCurrentIrql(void){return 0;}
static uint64_t fixture_time(void *p){(void)p;return 1;}
void NdisMIndicateStatusEx(NDIS_HANDLE h,NDIS_STATUS_INDICATION *v)
{assert(h&&v->StatusCode==NDIS_STATUS_DOT11_SCAN_CONFIRM&&*(NDIS_STATUS *)v->StatusBuffer==NDIS_STATUS_SUCCESS);confirmed++;}
static int direct_cmd(void *p,uint32_t a,uint32_t *v){(void)p;(void)a;*v=0;return 0;}
static int send_cmd(void *p,uint32_t a,unsigned wr,uint8_t *b,size_t n)
{(void)p;assert(wr&&n>=8&&((a>>28)&7)==1&&((a>>9)&0x1ffff)==0x34);assert(b[4]==3||b[4]==0x12||b[4]==0x13||b[4]==0x1c);sends++;return 0;}
static int success(void *p){(void)p;return 0;}
int main(void)
{
 uint8_t cis[]={0x20,4,0x7a,3,0x30,0x66,0xff};
 struct a04_wifi_owner owner={0,1,1,1,1,1,0,0};struct a04_wifi_life life={0};
 struct a04_sdio_direct direct_io={&owner,NULL,direct_cmd,0,0};struct a04_sdio_bus bus={NULL,send_cmd};
 struct a04_wifi_life_ops ops={NULL,success,success,success,success};struct a04_wmt_bridge b={0};struct a04_station_runtime r={0};
 assert(!a04_wifi_admit(&owner)&&!a04_wifi_life_begin(&life,&owner,cis,sizeof(cis),&direct_io,&bus,&ops));
 b.life=&life;b.wlan_ready=1;b.io.ctx=&b;b.io.command=command;b.io.delay=delay_us;
 b.transport.io=&b.io;b.transport.transfer=transfer;b.transport.enabled=2;b.transport.functions=2;
 b.wlan.transport=&b.transport;b.wlan.initialized=b.wlan.runtime=b.wlan.ready=1;b.wlan.cpu_free=26;
 r.bridge=&b;r.initialized=1;r.now=fixture_time;r.station.configured=1;
 struct a04_wlan_local *l=&r.station.local;l->bss=1;l->own=1;l->sta=2;l->wlan=3;l->bmc=19;l->permitted_channels=l->active_channels=1u<<6;l->mac[0]=2;l->mac[5]=1;
 struct a04_wlan_ap ap={0};ap.mac[0]=2;ap.mac[5]=2;ap.channel=6;ap.band=1;ap.beacon=100;ap.capability=1;ap.rate_count=1;ap.rates[0]=0x82;ap.ssid[0]='x';ap.ssid_length=1;
 assert(a04_wmt_bridge_station_select(&b,&r.station,&ap,0,1,100,1,100001)==A04_WLAN_STATE&&!sends);
 DOT11_SCAN_REQUEST_V2 req={0};struct a04_native_scan_policy policy={0};ULONG needed;
 req.dot11BSSType=dot11_BSS_type_any;memset(req.dot11BSSID,255,6);req.dot11ScanType=dot11_scan_type_passive;
 policy.bss=1;policy.sequence=7;policy.timeout=100;policy.channel_count=1;policy.channels[0]=1;policy.channels[1]=6;
 assert(!a04_native_scan_start(&b,&req,offsetof(DOT11_SCAN_REQUEST_V2,ucBuffer),&policy,&needed));
 assert(b.scan.pending&&!r.station.scan.complete);
 uint8_t event[]={12,0,0,0xe0,0x0d,0,0,0,7,0,0,0};assert(!packet(&r,event,sizeof(event)));
 assert(r.station.scan.complete&&r.station.scan.sequence==7&&!r.station.scan.pending);
 assert(!a04_native_scan_indicate(&b,&b)&&confirmed==1&&!b.scan.complete&&r.station.scan.complete);
 assert(!a04_wmt_bridge_station_select(&b,&r.station,&ap,0,1,100,1,100001));
 assert(sends==4&&r.station.selected&&r.station.channel_pending&&!r.station.sta_active&&!r.connected);
 /* Same SSID cache order must not choose a wrong security/channel profile. */
 assert(!a04_os_candidate_policy(&ap,l->active_channels,0));
 assert(a04_os_candidate_policy(&ap,l->active_channels,1)==A04_WLAN_SECURITY);
 unsigned n;const uint8_t *rsn=a04_wlan_rsn_selected(&n);memcpy(ap.rsn,rsn,n);ap.rsn_length=n;ap.capability|=16;
 assert(!a04_os_candidate_policy(&ap,l->active_channels,1));
 assert(a04_os_candidate_policy(&ap,l->active_channels,0)==A04_WLAN_SECURITY);
 assert(a04_os_candidate_policy(&ap,1u<<1,1)==A04_WLAN_UNSUPPORTED);
 ap.rsn[1]++;assert(a04_os_candidate_policy(&ap,l->active_channels,1)==A04_WLAN_SECURITY);
 return 0;
}
