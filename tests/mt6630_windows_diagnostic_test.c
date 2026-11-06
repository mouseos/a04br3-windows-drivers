/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <string.h>
#include "windows_diagnostic.h"
static int init_result,open_result,boot_result,stop_result,init_calls,open_calls,boot_calls,stop_calls,prepare_result,prepare_calls,releases;
int a04_windows_wifi_board_init(struct a04_windows_wifi_board *b,A04_SHARED_WIFI_INTERFACE *p,struct a04_windows_msdc2 *h,struct a04_wifi_owner *o,ULONG g)
{ (void)b;(void)p;(void)h;(void)o;(void)g;init_calls++;return init_result; }
int a04_windows_wifi_control_open(struct a04_windows_wifi_board *b,struct a04_wifi_board *x,struct a04_wifi_life *l,struct a04_wmt_bridge *w,uint32_t o,const struct a04_wifi_life_ops *ops,struct a04_sdio_card_info *c)
{ (void)b;(void)x;(void)w;(void)o;(void)ops;(void)c;open_calls++;if(!open_result)l->state=A04_WIFI_LIFE_IO;return open_result; }
int a04_windows_firmware_prepare(struct a04_windows_fw_snapshot *s,const uint8_t e[32]){(void)s;(void)e;prepare_calls++;return prepare_result;}
void a04_windows_firmware_release(struct a04_windows_fw_snapshot *s){(void)s;releases++;}
int a04_windows_firmware_boot_prepared(struct a04_wmt_bridge *b,struct a04_windows_fw_snapshot *s,const uint8_t e[32],const struct a04_wmt_startup_options *o)
{ (void)b;(void)s;(void)e;(void)o;boot_calls++;return boot_result; }
int a04_wifi_life_stop(struct a04_wifi_life *l){(void)l;stop_calls++;return stop_result;}
static int callback(void *p){(void)p;return 0;}
int main(void)
{
 static struct a04_windows_diagnostic d;A04_SHARED_WIFI_INTERFACE p={0};struct a04_windows_msdc2 h={0};struct a04_wifi_owner o={0};
 struct a04_wifi_life_ops ops={0};struct a04_wmt_startup_options opts={0};uint8_t hash[32]={0};
 ops.mask_and_drain_irq=ops.drain_io=ops.unmap_host=ops.release_lease=callback;
 assert(a04_windows_diagnostic_start(&d,&p,&h,&o,1,0,&ops,hash,&opts));assert(!init_calls);
 prepare_result=-1;assert(a04_windows_diagnostic_start(&d,&p,&h,&o,1,1,&ops,hash,&opts));
 assert(prepare_calls==1 && !init_calls && !open_calls && !boot_calls && !releases);
 memset(&d,0,sizeof(d));prepare_result=0;boot_result=-1;assert(a04_windows_diagnostic_start(&d,&p,&h,&o,1,1,&ops,hash,&opts));
 assert(d.state==A04_DIAG_FAILED && init_calls==1 && open_calls==1 && boot_calls==1);
 assert(!a04_windows_diagnostic_stop(&d));assert(stop_calls==1 && d.state==A04_DIAG_STOPPED);
 memset(&d,0,sizeof(d));d.state=A04_DIAG_FAILED;
 assert(a04_windows_diagnostic_stop(&d));assert(stop_calls==1);
 d.life.state=A04_WIFI_LIFE_FAILED;stop_result=-1;
 assert(a04_windows_diagnostic_stop(&d));assert(stop_calls==2 && d.state==A04_DIAG_FAILED);
 stop_result=0;
 memset(&d,0,sizeof(d));boot_result=0;assert(!a04_windows_diagnostic_start(&d,&p,&h,&o,1,1,&ops,hash,&opts));
 assert(d.state==A04_DIAG_READY && releases==2);assert(!a04_windows_diagnostic_stop(&d));assert(d.state==A04_DIAG_STOPPED && stop_calls==3);
 return 0;
}
