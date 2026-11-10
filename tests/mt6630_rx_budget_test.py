#!/usr/bin/env python3
"""実station poll関数のbounded drain/read-clear継続をhostで確認。RF成功ではない。"""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'drivers/network/mt6630/station_runtime.c').read_text()
start=s.index('int a04_station_runtime_poll(');end=s.index('\nint a04_station_runtime_connect(',start)
body=s[start:end]
# Deferred FIFO work must also prevent granting firmware ownership at idle.
assert '!b->pending_wlan_status&&!a->radio.rx_pending' in (root/'drivers/network/mt6630/ndis_runtime.c').read_text()
fixture=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define TRACE(s,k,v,p) ((void)0)
#define A04_WLAN_STATUS 84
struct a04_wmt_bridge {unsigned pending_wlan_status;struct {uint8_t rx[8];} wlan;};
struct a04_station_runtime {unsigned initialized,failed,rx_pending,rx_phase,rx_error,connecting,phase,connected;struct a04_wmt_bridge *bridge;struct {unsigned channel_pending,sta_active;} station;void *context;uint64_t(*now)(void*);};
static unsigned fifo[2],consumed[2],whisr,polls,credits,packets,acks;
static int fail(struct a04_station_runtime*r,int e){r->failed=e;return e;}
static int a04_wmt_bridge_poll_wlan_status(struct a04_wmt_bridge*b,uint32_t*s)
{assert(!b->pending_wlan_status);++polls;*s=whisr;whisr=0;b->pending_wlan_status=*s;return 0;}
static int a04_wlan_resources(void*h){(void)h;++credits;return 0;}
static int a04_wlan_read(void*h,unsigned reg,uint32_t*v)
{(void)h;assert(reg==0x90);*v=(fifo[0]?8:0)|((fifo[1]?8:0)<<16);return 0;}
static int a04_wmt_bridge_receive(struct a04_wmt_bridge*b,unsigned port,unsigned budget,const uint8_t**p,unsigned*n)
{assert(b->pending_wlan_status&&(b->pending_wlan_status&(2u<<port))&&budget==1&&fifo[port]);--fifo[port];++consumed[port];*p=b->wlan.rx;*n=8;return 0;}
static int packet(struct a04_station_runtime*r,const uint8_t*p,unsigned n){(void)r;(void)p;assert(n==8);++packets;return 0;}
static void a04_wlan_secret_clear(void*p,unsigned n){(void)p;assert(n==8);}
static int a04_wmt_bridge_ack_wlan_status(struct a04_wmt_bridge*b,unsigned status)
{assert(status&&status==b->pending_wlan_status);b->pending_wlan_status=0;++acks;return 0;}
static int a04_wmt_bridge_station_auth(struct a04_wmt_bridge*b,void*s,uint64_t t){(void)b;(void)s;(void)t;assert(0);return -1;}
''' + body + r'''
int main(void)
{
 struct a04_wmt_bridge b={0};struct a04_station_runtime r={.initialized=1,.bridge=&b};
 fifo[0]=100;fifo[1]=40;whisr=0x87;
 assert(!a04_station_runtime_poll(&r));assert(consumed[0]==32&&consumed[1]==32&&packets==64&&r.rx_pending==6&&credits==1&&acks==1&&!r.failed);
 /* Next WHISR is zero: actual unread RX is retained, no invented IRQ. */
 assert(!a04_station_runtime_poll(&r));assert(consumed[0]==64&&consumed[1]==40&&r.rx_pending==2&&credits==1&&acks==2);
 assert(!a04_station_runtime_poll(&r));assert(consumed[0]==96&&r.rx_pending==2);
 assert(!a04_station_runtime_poll(&r));assert(consumed[0]==100&&!r.rx_pending&&packets==140&&!r.failed&&acks==4);
 assert(!a04_station_runtime_poll(&r));assert(polls==5&&acks==4); /* empty means no ack */
 /* Fresh fault bits are still checked even while deferred FIFO is nonempty. */
 fifo[0]=65;whisr=2;assert(!a04_station_runtime_poll(&r));assert(r.rx_pending==2);
 unsigned before=packets;whisr=0x100;assert(a04_station_runtime_poll(&r)==A04_WLAN_STATUS);assert(r.failed&&packets==before);
 puts("PASS actual station poll: 140 RX, fair64 budget, deferred read-clear work, fresh faults; no HIF/RF execution");
}
'''
out=root/'build/mt6630-rx-budget';out.mkdir(parents=True,exist_ok=True)
(out/'boundary.c').write_text(fixture)
subprocess.run(['clang','-std=c11','-O1','-fsanitize=address,undefined','-Wall','-Wextra','-Werror',str(out/'boundary.c'),'-o',str(out/'boundary-test')],check=True)
subprocess.run([str(out/'boundary-test')],check=True)
