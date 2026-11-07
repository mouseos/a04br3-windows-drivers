/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/network/mt6630/wmt_bridge.c"
#include "native_scan_bridge.h"
#include "native_oid.h"
static int current_irql;
int KeGetCurrentIrql(void){return current_irql;}
struct fake {unsigned sends,indications;NDIS_STATUS expected;};static struct fake *active;
void NdisMIndicateStatusEx(NDIS_HANDLE handle,NDIS_STATUS_INDICATION *i)
{assert(handle==active && i->StatusCode==NDIS_STATUS_DOT11_SCAN_CONFIRM && i->StatusBufferSize==sizeof(NDIS_STATUS));assert(*(NDIS_STATUS *)i->StatusBuffer==active->expected);active->indications++;}
static int direct(void *c,uint32_t a,uint32_t *r){(void)c;(void)a;*r=0;return 0;}
static int send(void *context,uint32_t arg,unsigned write,uint8_t *data,size_t n)
{struct fake *f=context;assert(write && ((arg>>28)&7)==1 && ((arg>>9)&0x1ffff)==0x34 && n>=234);assert(data[4]==3 && data[5]==0xa0);assert(data[8+7]==0);assert(data[8+158]==4);f->sends++;return 0;}
static int done(void *c){(void)c;return 0;}
int main(void)
{
 uint8_t cis[]={0x20,4,0x7a,3,0x30,0x66,0xff};uint8_t event[]={12,0,0,0xe0,0x0d,0,0,0,7,0,0,0};
 struct a04_wifi_owner o={0,1,1,1,1,1,0,0};struct a04_wifi_life life={0};struct fake f={0};
 struct a04_sdio_direct d={&o,&f,direct,0,0};struct a04_sdio_bus bus={&f,send};struct a04_wifi_life_ops ops={&f,done,done,done,done};
 struct a04_wmt_bridge b={0};DOT11_SCAN_REQUEST_V2 request;struct a04_native_scan_policy p={0};ULONG needed;NDIS_OID_REQUEST oid={0};struct a04_native_oid_context oidctx;
 memset(&request,0,sizeof(request));request.dot11BSSType=dot11_BSS_type_any;memset(request.dot11BSSID,0xff,6);request.dot11ScanType=dot11_scan_type_passive;
 p.bss=1;p.sequence=7;p.min_dwell=10;p.dwell=20;p.timeout=100;p.channel_count=1;p.channels[0]=1;p.channels[1]=6;
 assert(!a04_wifi_admit(&o));assert(!a04_wifi_life_begin(&life,&o,cis,sizeof(cis),&d,&bus,&ops));
 b.life=&life;b.wlan_ready=1;b.io.ctx=&b;b.io.command=command;b.io.delay=delay_us;b.transport.io=&b.io;b.transport.transfer=transfer;b.transport.enabled=2;b.transport.functions=2;
 b.wlan.transport=&b.transport;b.wlan.initialized=1;b.wlan.runtime=1;b.wlan.cpu_free=8;active=&f;
 assert(a04_native_scan_indicate(&b,&f)==NDIS_STATUS_INVALID_DATA && !f.indications);
 oidctx.bridge=&b;oidctx.policy=p;oid.RequestType=NdisRequestSetInformation;oid.DATA.SET_INFORMATION.Oid=OID_DOT11_SCAN_REQUEST;
 oid.DATA.SET_INFORMATION.InformationBuffer=&request;oid.DATA.SET_INFORMATION.InformationBufferLength=(ULONG)offsetof(DOT11_SCAN_REQUEST_V2,ucBuffer);
 current_irql=2;assert(a04_native_oid_dispatch(&oidctx,&oid)==NDIS_STATUS_NOT_SUPPORTED && !f.sends);current_irql=0;
 assert(!a04_native_oid_dispatch(&oidctx,&oid));assert(oid.DATA.SET_INFORMATION.BytesRead==oid.DATA.SET_INFORMATION.InformationBufferLength);
 assert(f.sends==1 && b.scan.pending && !f.indications);
 assert(a04_native_scan_start(&b,&request,(ULONG)offsetof(DOT11_SCAN_REQUEST_V2,ucBuffer),&p,&needed)==NDIS_STATUS_DOT11_MEDIA_IN_USE);
 assert(!a04_wmt_bridge_scan_event(&b,event,sizeof(event)));assert(!a04_native_scan_indicate(&b,&f) && f.indications==1);
 assert(a04_native_scan_indicate(&b,&f)==NDIS_STATUS_INVALID_DATA && f.indications==1);
 p.sequence=8;assert(!a04_native_scan_start(&b,&request,(ULONG)offsetof(DOT11_SCAN_REQUEST_V2,ucBuffer),&p,&needed));
 f.expected=NDIS_STATUS_FAILURE;assert(!a04_native_scan_fail(&b,&f,NDIS_STATUS_FAILURE) && f.indications==2 && o.poisoned);
 assert(a04_native_scan_fail(&b,&f,NDIS_STATUS_FAILURE)==NDIS_STATUS_INVALID_DATA && f.indications==2);
 /* Directly inspect the published gen3 four-SSID body, not four separate
  * scans or an inflated advertised list capacity. */
 uint8_t payload[826],names[4][32]={{'a'},{'b','b'},{'c','c','c'},{'d','d','d','d'}};unsigned lengths[4]={1,2,3,4},wire_bytes=0;
 struct a04_wlan_scan scan={0};scan.wire_version=2;scan.active=1;scan.sequence=9;scan.bss=1;scan.ssid_count=4;scan.ssid_list=&names[0][0];scan.ssid_lengths=lengths;scan.timeout=2000;
 assert(!a04_wlan_scan_gen3_encode(&scan,payload,sizeof(payload),&wire_bytes));assert(wire_bytes==226&&payload[4]==4&&payload[7]==0);
 for(unsigned i=0;i<4;i++){assert(a04_wmt_get32(payload+8+36*i)==lengths[i]);assert(!memcmp(payload+12+36*i,names[i],lengths[i]));}
 scan.ssid_count=5;assert(a04_wlan_scan_gen3_encode(&scan,payload,sizeof(payload),&wire_bytes));
 puts("Native scan to SDIO and matching event fixture: OK");return 0;
}
