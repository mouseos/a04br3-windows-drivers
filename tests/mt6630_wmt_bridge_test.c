/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdio.h>
#include "../drivers/network/mt6630/wmt_bridge.c"
struct fake {unsigned requests,services;uint32_t status;};
static int direct(void *context,uint32_t arg,uint32_t *out){(void)context;(void)arg;*out=0;return 0;}
static int transfer_fake(void *context,uint32_t arg,unsigned write,uint8_t *data,size_t n)
{
 struct fake *f=context;unsigned address=(arg>>9)&0x1ffffu;uint32_t v;
 assert(!write && n==4 && ((arg>>28)&7)==1);f->requests++;
 assert(address==0x10 || address==0x20);v=address==0x10?f->status:0;
 data[0]=(uint8_t)v;data[1]=(uint8_t)(v>>8);data[2]=(uint8_t)(v>>16);data[3]=(uint8_t)(v>>24);return 0;
}
static int service(void *p){((struct fake *)p)->services++;return 0;}
static int done(void *context){(void)context;return 0;}
int main(void)
{
 uint8_t cis[]={0x20,4,0x7a,0x03,0x30,0x66,0xff};uint32_t status=0;
 struct a04_wifi_owner owner={0,1,1,1,1,1,0,0};struct a04_wifi_life life={0};struct fake f={0};
 struct a04_sdio_direct d={&owner,&f,direct,0,0};struct a04_sdio_bus bus={&f,transfer_fake};
 struct a04_wifi_life_ops ops={&f,done,done,done,done};struct a04_wmt_bridge b={0};
 assert(!a04_wifi_admit(&owner));assert(!a04_wifi_life_begin(&life,&owner,cis,sizeof(cis),&d,&bus,&ops));
 b.life=&life;b.wlan_ready=1;b.io.ctx=&b;b.io.command=command;b.io.delay=delay_us;
 b.transport.io=&b.io;b.transport.transfer=transfer;b.transport.enabled=2;b.transport.functions=2;
 b.wlan.transport=&b.transport;b.service_context=&f;b.service_other=service;f.status=3;
 assert(!a04_wmt_bridge_poll_wlan_status(&b,&status)&&status==3&&f.requests==1&&f.services==1);
 assert(a04_wmt_bridge_poll_wlan_status(&b,&status)<0&&f.requests==1&&f.services==1);
 assert(a04_wmt_bridge_ack_wlan_status(&b,1)<0);assert(!a04_wmt_bridge_ack_wlan_status(&b,3));
 {uint8_t event[12]={12,0,0,0xe0,0x0d,0,0,0,7,0,0,0};
  b.scan.pending=1;b.scan.sequence=7;
  assert(!a04_wmt_bridge_scan_event(&b,event,sizeof(event)) && b.scan.complete);
  assert(!a04_wmt_bridge_scan_consume(&b) && !b.scan.complete);
 }
 f.status=0x40;assert(a04_wmt_bridge_poll_wlan_status(&b,&status)!=0);
 assert(b.failed && owner.poisoned && life.state==A04_WIFI_LIFE_FAILED && f.requests==3 && f.services==2);
 puts("WLAN read-clear status accounting and poison: OK");return 0;
}
