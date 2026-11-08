/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef unsigned char UCHAR;
#define CTL_CODE(a,b,c,d) (b)
#define FILE_DEVICE_UNKNOWN 0
#define METHOD_BUFFERED 0
#define METHOD_NEITHER 0
#define FILE_READ_DATA 0
#define FILE_WRITE_DATA 0
#include "../drivers/bluetooth/mt6630/broker_abi.h"
#include "../drivers/bluetooth/mt6630/transport_caps.h"
int main(void){
 struct a04_mt6630_route_facts ok={1,1,0,1,1,12,12,1,1,1,1},f;
 assert(a04_mt6630_route_ready(&ok));
 unsigned *fields[]={&ok.merge,&ok.firmware_ready,&ok.stp_ready,&ok.phase_ready,&ok.radio_ready,&ok.version_valid,&ok.pcm_ack,&ok.epoch_live};
 for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);i++){*fields[i]=0;assert(!a04_mt6630_route_ready(&ok));*fields[i]=1;}
 f=ok;f.firmware_fault=1;assert(!a04_mt6630_route_ready(&f));f=ok;f.acked--;assert(!a04_mt6630_route_ready(&f));
 struct a04_bt_transport_caps c={1,1,7,A04_BT_ROUTE_HCI_BYPASS};assert(sizeof(c)==16&&sizeof(struct a04_bt_broker_state)==16&&a04_mt6630_transport_bypass(&c));
 c.version=2;assert(!a04_mt6630_transport_bypass(&c));c.version=1;c.generation=0;assert(!a04_mt6630_transport_bypass(&c));c.generation=7;c.routes=0;assert(!a04_mt6630_transport_bypass(&c));
 puts("PASS production live transport-route proof/refusals and v1 record: no Audio/PCM latency callback or HFP peer is an input; mock facts, not hardware");
}
