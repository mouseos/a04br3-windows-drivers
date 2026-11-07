/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "native_policy.h"
#include "vendor_xnu/A04BR3WLANStation.h"
#include <string.h>
struct lp_fixture {unsigned driver,refuse_sleep,refuse_wake,requests,transfers,delays;uint8_t command[20];};
static int lp_command(void *context,unsigned op,uint32_t arg,unsigned response,uint32_t *out)
{
 struct lp_fixture *f=context;unsigned value=arg&255;
 assert(op==52&&response==1&&(arg>>31)&&((arg>>28)&7)==1&&((arg>>9)&0x1ffff)==5);
 assert(value==1||value==2);f->requests++;
 if(value==1&&!f->refuse_sleep)f->driver=0;
 if(value==2&&!f->refuse_wake)f->driver=1;
 *out=0;return 0;
}
static int lp_transfer(void *context,uint32_t arg,void *bytes,unsigned length)
{
 struct lp_fixture *f=context;uint8_t *p=bytes;unsigned reg=(arg>>9)&0x1ffff;
 assert(((arg>>28)&7)==1);f->transfers++;
 if(arg>>31){assert(reg==0x34&&length==20);memcpy(f->command,p,20);}
 else {assert(reg==4&&length==4);memset(p,0,4);p[1]=(uint8_t)f->driver;}
 return 0;
}
static void lp_delay(void *context,unsigned us)
{struct lp_fixture *f=context;assert(us==1000);f->delays++;}
static void actual_lp_contract(void)
{
 struct lp_fixture f={.driver=1};struct a04_sdio_io io={.ctx=&f,.command=lp_command,.delay=lp_delay};
 struct a04_sdio_transport transport={.io=&io,.transfer=lp_transfer,.functions=2,.enabled=2};
 struct a04_wlan_host h={.transport=&transport,.initialized=1,.ready=1,.runtime=1,.cpu_free=26,.tc0_free=13};
 struct a04_wlan_local local={.bss=1,.own=2,.bmc=19,.sta=0,.wlan=0,.permitted_channels=2,.active_channels=2,.mac={2,1,2,3,4,5}};
 uint8_t body[12];
 assert(!a04_wlan_deactivate_encode(&local,body));
 assert(body[0]==1&&body[3]==2);for(unsigned i=0;i<12;i++)if(i!=0&&i!=3)assert(!body[i]);
 assert(!a04_wlan_set(&h,0x11,body,12,NULL));
 assert(f.command[4]==0x11&&f.command[6]==1&&!memcmp(f.command+8,body,12));
 unsigned requests=f.requests;
 assert(a04_wlan_firmware_own(&h)==A04_WLAN_STATE&&f.requests==requests); /* outstanding CPU page */
 assert(!a04_wlan_credit_return(&h,0,0,1,1));
 assert(!a04_wlan_firmware_own(&h)&&h.fw_owned&&!f.driver);
 unsigned transfers=f.transfers;uint32_t value;
 assert(a04_wlan_read(&h,0x10,&value)==A04_WLAN_STATE&&f.transfers==transfers);
 assert(a04_wlan_send(&h,20,1)==A04_WLAN_STATE&&f.transfers==transfers); /* no HIF while idle */
 assert(!a04_wlan_driver_own(&h)&&!h.fw_owned&&f.driver);
 assert(!a04_wlan_activate_encode(&local,1,body)&&!a04_wlan_set(&h,0x11,body,12,NULL));
 assert(f.command[9]==1&&f.command[11]==2&&f.command[18]==19&&!memcmp(f.command+12,local.mac,6));
 assert(!a04_wlan_credit_return(&h,0,0,1,1));
 f.refuse_sleep=1;
 assert(a04_wlan_firmware_own(&h)==A04_WLAN_STATE&&!h.fw_owned&&f.driver); /* refusal is not success */
 f.refuse_sleep=0;assert(!a04_wlan_firmware_own(&h));
 f.refuse_wake=1;
 assert(a04_wlan_driver_own(&h)==A04_WLAN_TIMEOUT&&h.fw_owned&&h.failed&&f.delays==2048);
 assert(a04_wlan_send(&h,20,1)==A04_WLAN_STATE);
}
int main(void)
{
 actual_lp_contract();
 unsigned profile=99;
 assert(!a04_native_power_profile(0,&profile)&&profile==0);
 assert(!a04_native_power_profile(8,&profile)&&profile==2);
 assert(!a04_native_power_profile(16,&profile)&&profile==1);
 assert(a04_native_power_profile(24,&profile)==-2&&profile==1);
 for(unsigned i=1;i<24;i++)if(i!=8&&i!=16)assert(a04_native_power_profile(i,&profile)==-1&&profile==1);
 assert(a04_native_power_profile(UINT32_MAX,&profile)==-1&&profile==1);
 struct a04_peer_loss loss={A04_UNREACHABLE_DEFAULT_MS,100,0};
 assert(!a04_peer_loss_expired(&loss,UINT64_MAX)); /* silence is not evidence */
 loss.suspected=1;
 assert(!a04_peer_loss_expired(&loss,99)); /* monotonic guard */
 assert(!a04_peer_loss_expired(&loss,2000099)&&a04_peer_loss_expired(&loss,2000100));
 a04_peer_observed(&loss,2000100);
 assert(!a04_peer_loss_expired(&loss,9000000)); /* recovered RX cancels evidence */
 loss.suspected=1;loss.threshold_ms=7000;
 assert(!a04_peer_loss_expired(&loss,9000099)&&a04_peer_loss_expired(&loss,9000100));
 loss.threshold_ms=0;assert(a04_peer_loss_expired(&loss,2000100));
 loss.threshold_ms=UINT32_MAX;
 assert(!a04_peer_loss_expired(&loss,loss.last_rx_us+(uint64_t)UINT32_MAX*1000-1));
 assert(a04_peer_loss_expired(&loss,loss.last_rx_us+(uint64_t)UINT32_MAX*1000));
 puts("Native power policy, real LP/BSS wire transitions and bounded failures: OK");
 return 0;
}
