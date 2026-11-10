#!/usr/bin/env python3
"""Actual STOP and disassociation functions with mocked register/event I/O."""
import argparse
from pathlib import Path
import subprocess
parser=argparse.ArgumentParser();parser.add_argument('--output-dir',required=True,type=Path);args=parser.parse_args();r=Path(__file__).resolve().parents[1]
def fn(file,sig):
 s=(r/'drivers/network/mt6630'/file).read_text();a=s.index(sig);end=s.index('{',a)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[a:end]
body=fn('wmt_bridge.c','static int transfer(')+'\n'+fn('wmt_bridge.c','static int alive(')+'\n'+fn('wmt_bridge.c','int a04_wmt_bridge_wlan_stop(')+'\n'+fn('ndis_runtime.c','static int wlan_reset_disassociate(')
prefix=r'''
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>
#define A04_WIFI_LIFE_IO 1
#define A04_WLAN_STATE 87
#define A04_WLAN_FORMAT 81
#define A04_WLAN_TIMEOUT 83
#define A04_WLAN_STATUS 84
#define NdisMoveMemory memcpy
struct owner{unsigned active,poisoned,generation;};struct life{unsigned state,generation;struct owner*owner;};
struct a04_wlan_host{unsigned runtime,fw_owned;int failed;uint8_t tx[128],rx[128];};
struct a04_wmt_bridge{struct life*life;unsigned wlan_ready,failed,pending_wlan_status,calibration_submitted,wlan_reset_service;
 struct a04_wlan_host wlan;struct{unsigned failed;}transport;
 struct{unsigned attempted;}wlan_state;struct{unsigned pending;}scan;
 int(*service_other)(void*);void*service_context;void(*delay)(void*,unsigned);void*delay_context;};
struct a04_wlan_local{unsigned wlan,own;uint8_t mac[6];};
struct a04_wlan_station{struct a04_wlan_local local;struct{uint8_t mac[6];}ap;unsigned sta_active;};
struct a04_ndis_runtime{struct{struct a04_wlan_station station;struct a04_wmt_bridge*bridge;}radio;
 struct{struct{unsigned next_pid;}state;}tx;unsigned reset_mgmt_pid,reset_mgmt_status,reset_mgmt_pending,closing;};
static unsigned mode,reads,services,power_cmd,own_req,sends,rx_packets,transfers;
static uint64_t ticks;
static void a04_wlan_zero(void*p,unsigned n){memset(p,0,n);}
static void a04_wlan_secret_clear(void*p,unsigned n){memset(p,0,n);}
static int a04_wlan_set(struct a04_wlan_host*h,unsigned cid,const uint8_t*p,unsigned n,void*response){(void)h;assert(cid==4&&n==4&&p[0]==1&&!p[1]&&!p[2]&&!p[3]&&!response);power_cmd++;return mode==1?-11:0;}
static int a04_wlan_interrupt_status(struct a04_wlan_host*h,uint32_t*v){(void)h;*v=1;return 0;}
static int a04_wlan_resources(struct a04_wlan_host*h){(void)h;return mode==7?-17:0;}
static int a04_wlan_receive(struct a04_wlan_host*h,unsigned port,unsigned polls){(void)h;assert(port<2&&polls==1);rx_packets++;return 0;}
static int a04_wlan_read(struct a04_wlan_host*h,unsigned reg,uint32_t*v){(void)h;
 if(reg==0){reads++;*v=(mode==3?0x1234:0x6630)|((mode==2||reads<3)?1u<<21:0);}
 else if(reg==4)*v=mode==4?0x100:0;
 else {assert(reg==0x90);*v=0;}
 return mode==5?-15:0;}
static int a04_wlan_lp_request(struct a04_wlan_host*h,unsigned req){(void)h;assert(req==1);own_req++;return 0;}
static int a04_wifi_life_cmd53(struct life*l,unsigned write,unsigned fn,unsigned incr,unsigned addr,void*buf,unsigned len){(void)write;(void)incr;(void)addr;assert(l&&fn&&buf&&len);transfers++;return 0;}
static int service(void*p){(void)p;services++;return mode==6?-16:0;}
static void delay(void*p,unsigned us){(void)p;assert(us==1000);ticks+=us;}
static int a04_wlan_mac_valid(const uint8_t*p){return p&&p[0]&&!(p[0]&1);}
static int a04_wlan_local_valid(const struct a04_wlan_local*l){return l&&l->wlan<18&&l->own<4;}
static int a04_wlan_rate_mask(const void*ap,unsigned*a,unsigned*b){(void)ap;*a=*b=1;return 0;}
static unsigned a04_wlan_basic_rate_code(unsigned b){assert(b==1);return 2;}
static void a04_wmt_put16(uint8_t*p,unsigned x){p[0]=x;p[1]=x>>8;}
static int a04_wlan_send(struct a04_wlan_host*h,unsigned n,unsigned tc){assert(n==54&&tc==1);sends++;
 assert(h->tx[0]==54&&h->tx[3]==0x88&&h->tx[4]==3&&h->tx[5]==0xcc&&h->tx[7]==4);
 assert(h->tx[8]==10&&h->tx[20]==1&&h->tx[21]==2&&h->tx[26]==8);
 assert(h->tx[28]==0xa0&&h->tx[29]==0&&h->tx[52]==8&&h->tx[53]==0);
 assert(!memcmp(h->tx+32,h->tx+44,6)&&h->tx[32]==2&&h->tx[38]==4);
 return mode==1?-11:0;}
static uint64_t now(void*p){(void)p;return ticks;}
static void wait_us(void*p,unsigned us){(void)p;ticks+=us;}
static int a04_ndis_poll(struct a04_ndis_runtime*a){if(mode==5)return -15;if(mode!=2){a->reset_mgmt_pending=0;a->reset_mgmt_status=mode==4?7:mode==6?1:0;}return 0;}
'''
suffix=r'''
static void init(struct a04_wmt_bridge*b,struct life*l){memset(b,0,sizeof(*b));b->life=l;b->wlan_ready=b->wlan.runtime=b->calibration_submitted=1;b->wlan_state.attempted=1;b->service_other=service;b->delay=delay;reads=services=power_cmd=own_req=sends=rx_packets=0;ticks=0;mode=0;}
int main(void){struct owner owner={1,0,7};struct life life={1,7,&owner};struct a04_wmt_bridge b;
 init(&b,&life);assert(!a04_wmt_bridge_wlan_stop(&b));assert(reads==3&&services==3&&power_cmd==1&&own_req==1&&!b.wlan_ready&&!b.wlan_state.attempted&&b.wlan.fw_owned);
 for(unsigned m=1;m<=7;m++){init(&b,&life);mode=m;assert(a04_wmt_bridge_wlan_stop(&b)!=0);assert(b.wlan_ready&&b.wlan_state.attempted&&b.calibration_submitted&&!b.wlan.fw_owned);}
 init(&b,&life);b.scan.pending=1;assert(a04_wmt_bridge_wlan_stop(&b)!=0&&!power_cmd);
 init(&b,&life);unsigned char byte=0;transfers=0;b.wlan_reset_service=1;
 assert(!transfer(&b,1u<<28,&byte,1)&&services==1&&transfers==1);
 assert(!transfer(&b,2u<<28,&byte,1)&&services==1&&transfers==2);
 b.wlan_reset_service=0;assert(!transfer(&b,1u<<28,&byte,1)&&services==1&&transfers==3);
 b.wlan_reset_service=1;mode=6;assert(transfer(&b,1u<<28,&byte,1)==-16&&transfers==3);

 for(unsigned m=0;m<=6;m++){
  init(&b,&life);mode=m;struct a04_ndis_runtime a={0};a.radio.bridge=&b;a.radio.station.sta_active=1;a.radio.station.local.wlan=3;a.radio.station.local.own=1;a.radio.station.local.mac[0]=4;a.radio.station.ap.mac[0]=2;
  int e=wlan_reset_disassociate(&a);if(m==0||m==3||m==6)assert(!e);else assert(e);assert(sends==1);
 }
 return 0;}
'''
args.output_dir.mkdir(parents=True,exist_ok=True);c=args.output_dir/'wire-host.c';exe=args.output_dir/'wire-host';c.write_text(prefix+body+suffix)
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(c),'-o',str(exe)],check=True);subprocess.run([str(exe.resolve())],check=True)
print('PASS actual STOP/Disassociation: real-read gating, timeout/errors, shared service, descriptor/body/PID, TX_DONE requirement; mock I/O')
