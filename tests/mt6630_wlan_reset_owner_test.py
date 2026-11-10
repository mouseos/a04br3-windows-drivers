#!/usr/bin/env python3
"""Actual owner function, mocked hardware/NDIS; no Windows driver build."""
import argparse
from pathlib import Path
import subprocess
parser=argparse.ArgumentParser();parser.add_argument('--output-dir',required=True,type=Path);args=parser.parse_args()
r=Path(__file__).resolve().parents[1]
s=(r/'drivers/network/mt6630/ndis_runtime.c').read_text()
start=s.index('int a04_ndis_wlan_reset(');end=s.index('int a04_ndis_radio_reset(',start)
owner=s[start:end]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "owned_queue.h"
typedef unsigned ULONG;typedef uint64_t ULONGLONG;
struct nbl{unsigned completed,status;};typedef struct nbl *PNET_BUFFER_LIST;
#define PASSIVE_LEVEL 0
#define NDIS_STATUS_RESET_IN_PROGRESS 0xc001000du
#define NDIS_STATUS_DOT11_DISASSOCIATION 1
#define DOT11_ASSOC_STATUS_DISASSOCIATED_BY_OS 7
#define NDIS_OBJECT_TYPE_DEFAULT 128
#define A04_WLAN_STATE 87
#define A04_WLAN_TIMEOUT 83
#define A04_SESSION_OFF 0
#define A04_SESSION_STOPPED 4
#define NdisZeroMemory(p,n) memset(p,0,n)
#define RtlSecureZeroMemory(p,n) memset(p,0,n)
#define NdisMoveMemory(p,q,n) memcpy(p,q,n)
#define NdisAcquireSpinLock(p) ((void)(p))
#define NdisReleaseSpinLock(p) ((void)(p))
struct a04_wlan_local{unsigned bss,own,sta,wlan,bmc;};
struct host{int failed;};
struct a04_wmt_bridge{unsigned failed;struct{unsigned pending;}scan;struct host wlan;struct{int failed;}transport;unsigned bt_marker,wlan_reset_service;};
struct a04_wlan_scan_state{unsigned pending,complete;int failed;};
struct station{struct a04_wlan_scan_state scan;struct a04_wlan_local local;struct{uint8_t mac[6];}ap;unsigned configured;};
struct a04_windows_fw_snapshot{uint8_t *wmt,*wlan;unsigned wmt_length,wlan_length;};
typedef struct{struct{unsigned Type,Revision,Size;}Header;uint8_t MacAddr[6];unsigned uReason;}DOT11_DISASSOCIATION_PARAMETERS;
struct a04_ndis_runtime{
 struct{struct a04_wmt_bridge *bridge;struct station station;unsigned connected,connecting,initialized,selected_bssid_set,rx_pending,phase;int failed;
 struct{unsigned bss,own,sta,wtbl;}slots;uint32_t cancelled_scan_sequences[8];}radio;
 struct{ULONG auth,unicast,multicast,default_id;unsigned exclude_unencrypted,keys,exemptions;}os_security;
 struct{PNET_BUFFER_LIST pending;struct{unsigned next_pid;int failed;}state;}tx;
 struct{unsigned state;}session;struct{unsigned suspected;}peer_loss;
 unsigned reset_in_progress,reset_mgmt_pending,reset_mgmt_pid,reset_mgmt_status,power,uncertain,closing,ps_activity;
 unsigned association_reported,early_length,eapol_tx_observed,peer_reset_pending,rx_count,idle_bss,power_request,media_streaming;
 unsigned lock,stats,association_base;uint8_t early_rx[16];struct{uint8_t bytes[16];unsigned length;}rx[2];
 PNET_BUFFER_LIST send;struct a04_owned_queue send_queue;
};
static unsigned fail_stage,stage,stopped,released,disassoc,txcount,disassoc_tx;
static uint64_t ticks;
static int step(unsigned n){stage=n;return fail_stage==n?-(int)n:0;}
static unsigned KeGetCurrentIrql(void){return 0;}
static int a04_wlan_local_valid(const struct a04_wlan_local*l){return l->bss<4&&l->bmc>=19;}
static int a04_windows_firmware_prepare_wlan(struct a04_windows_fw_snapshot*f){int e=step(1);if(!e)f->wlan=(void*)1;return e;}
static int a04_wlan_driver_own(struct host*h){(void)h;return step(2);}
static void finish_tx(struct a04_ndis_runtime*a,PNET_BUFFER_LIST n,unsigned status,unsigned flags){(void)a;(void)flags;assert(!n->completed);n->completed++;n->status=status;assert(status==NDIS_STATUS_RESET_IN_PROGRESS);txcount++;}
static uint64_t now(void*p){(void)p;return ticks;}
static void wait_us(void*p,unsigned us){(void)p;ticks+=us;}
static int a04_ndis_poll(struct a04_ndis_runtime*a){int e=step(3);if(e)return e;if(a->tx.pending&&fail_stage!=99){PNET_BUFFER_LIST n=a->tx.pending;a->tx.pending=NULL;finish_tx(a,n,NDIS_STATUS_RESET_IN_PROGRESS,0);}return 0;}
static int wlan_reset_disassociate(struct a04_ndis_runtime*a){assert(a->reset_in_progress);disassoc_tx++;return step(4);}
static int a04_wmt_bridge_wlan_stop(struct a04_wmt_bridge*b){assert(b->bt_marker==0x6630);int e=step(5);if(!e)stopped=1;return e;}
static void a04_os_security_clear(struct a04_ndis_runtime*a){assert(stopped||!a->power);memset(&a->os_security,0,sizeof(a->os_security));}
static void a04_ndis_status(struct a04_ndis_runtime*a,unsigned status,void*p,unsigned bytes){(void)p;(void)bytes;assert(status==1&&a->reset_in_progress&&stopped);disassoc++;}
static int a04_windows_firmware_boot_wlan_prepared(struct a04_wmt_bridge*b,struct a04_windows_fw_snapshot*f){assert(stopped&&f->wlan&&b->bt_marker==0x6630);return step(6);}
static int a04_windows_calibration_load(struct a04_wmt_bridge*b){(void)b;return step(7);}
static int a04_wlan_native_be_enable(struct host*h){(void)h;return step(8);}
static int a04_wmt_bridge_station_setup(struct a04_wmt_bridge*b,struct station*s,const struct a04_wlan_local*l){(void)b;int e=step(9);if(!e){s->local=*l;s->configured=1;}return e;}
static int a04_ndis_power_profile(struct a04_ndis_runtime*a,unsigned req,unsigned streaming,int force){assert(a->reset_in_progress&&req==8&&streaming==1&&!force);return step(10);}
static void a04_windows_firmware_release(struct a04_windows_fw_snapshot*f){f->wlan=NULL;released++;}
'''
suffix=r'''
static void init(struct a04_ndis_runtime*a,struct a04_wmt_bridge*b){
 memset(a,0,sizeof(*a));memset(b,0,sizeof(*b));b->bt_marker=0x6630;a->radio.bridge=b;a->radio.initialized=1;a->power=1;
 a->radio.station.local=(struct a04_wlan_local){0,0,0,0,19};a->radio.station.scan.complete=1;a->os_security.auth=7;a->os_security.unicast=a->os_security.multicast=4;
 a->os_security.default_id=2;a->os_security.exclude_unencrypted=1;a->os_security.keys=5;a->os_security.exemptions=2;
 a->power_request=8;a->media_streaming=1;a->stats=99;a->rx_count=1;a->rx[0].length=8;a->early_length=5;a->association_reported=1;
 fail_stage=stage=stopped=released=disassoc=txcount=disassoc_tx=0;ticks=0;
}
int main(void){
 struct a04_ndis_runtime a;struct a04_wmt_bridge b;
 init(&a,&b);struct nbl n[3]={{0}};a.send=&n[0];a.send_queue.inflight=1;assert(!a04_queue_put(&a.send_queue,&n[1]));assert(!a04_queue_put(&a.send_queue,&n[2]));
 assert(!a04_ndis_wlan_reset(&a));assert(stopped&&stage==10&&released==1&&txcount==3);
 assert(!a.radio.connected&&a.radio.initialized&&a.radio.station.configured&&a.radio.station.scan.complete&&a.reset_in_progress);
 assert(a.os_security.auth==7&&a.os_security.unicast==4&&a.os_security.default_id==2&&!a.os_security.keys&&!a.os_security.exemptions);
 assert(!a.stats&&!a.rx_count&&!a.early_length&&a.power&&b.bt_marker==0x6630&&!a.send&&!a.send_queue.count&&!a.send_queue.inflight);
 init(&a,&b);a.radio.connected=1;assert(!a04_ndis_wlan_reset(&a));assert(disassoc_tx==1&&disassoc==1);
 for(unsigned failure=1;failure<=10;failure++){
  init(&a,&b);a.radio.connected=1;fail_stage=failure;assert(a04_ndis_wlan_reset(&a)!=0);assert(b.bt_marker==0x6630);
  if(failure<=5)assert(a.os_security.keys==5);else assert(!a.os_security.keys);
  if(failure>1)assert(a.radio.failed&&released==1);else assert(!a.reset_in_progress&&!released);
 }
 init(&a,&b);struct nbl pending={0};a.send=a.tx.pending=&pending;a.send_queue.inflight=1;fail_stage=99;
 assert(a04_ndis_wlan_reset(&a)==A04_WLAN_TIMEOUT);assert(!a.tx.pending&&pending.completed==1&&a.radio.failed&&!stopped);
 init(&a,&b);a.power=0;a.session.state=A04_SESSION_STOPPED;assert(!a04_ndis_wlan_reset(&a));assert(!stage&&!a.os_security.keys&&!stopped&&!a.power);
 init(&a,&b);a.radio.station.scan.pending=1;assert(!a04_ndis_wlan_reset(&a)&&!a.radio.station.scan.complete);
 init(&a,&b);a.radio.station.scan.failed=1;assert(!a04_ndis_wlan_reset(&a)&&!a.radio.station.scan.complete);
 init(&a,&b);b.scan.pending=1;assert(a04_ndis_wlan_reset(&a)!=0&&!stage&&!a.reset_in_progress);
 return 0;
}
'''
args.output_dir.mkdir(parents=True,exist_ok=True);c=args.output_dir/'owner-host.c';exe=args.output_dir/'owner-host'
c.write_text(prefix+owner+suffix)
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(r/'drivers/network/mt6630'),str(c),'-o',str(exe)],check=True)
subprocess.run([str(exe.resolve())],check=True)
print('PASS actual WLAN owner: powered/OFF/connected, queue exactly-once, key epoch, all10 failures, unknown-TX safe completion; mocked hardware')
