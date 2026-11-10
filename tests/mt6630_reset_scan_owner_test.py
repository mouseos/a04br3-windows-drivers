#!/usr/bin/env python3
"""Actual queued RESET validation/cancel handoff, mocked NDIS and HIF."""
import argparse
from pathlib import Path
import subprocess
parser=argparse.ArgumentParser();parser.add_argument('--output-dir',required=True,type=Path);args=parser.parse_args();r=Path(__file__).resolve().parents[1]
s=(r/'drivers/network/mt6630/native_control.c').read_text();a=s.index('static int reset_queued(');b=s.index('static NDIS_STATUS scan(',a);body=s[a:b]
prefix=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#define NdisAcquireSpinLock(p) ((void)(p))
#define NdisReleaseSpinLock(p) ((void)(p))
#define NdisMoveMemory memcpy
#define NdisRequestMethod 12
#define OID_DOT11_RESET_REQUEST 0x0d010310
#define dot11_reset_type_phy_and_mac 3
#define NDIS_STATUS_FAILURE 0xc0000001u
#define NDIS_STATUS_REQUEST_ABORTED 0xc001000cu
#define NDIS_STATUS_PENDING 0x103u
#define NDIS_STATUS_DOT11_SCAN_CONFIRM 1
typedef unsigned ULONG,NDIS_STATUS;
typedef struct{unsigned dot11ResetType;uint8_t dot11MacAddress[6],bSetDefaultMIB;}DOT11_RESET_REQUEST;
typedef struct{unsigned x,y;}DOT11_STATUS_INDICATION;
struct q{unsigned RequestType;struct{struct{unsigned Oid,InputBufferLength,OutputBufferLength;void*InformationBuffer;}METHOD_INFORMATION;}DATA;};typedef struct q*PNDIS_OID_REQUEST;
struct a04_wmt_bridge{struct{unsigned sequence,bss,pending,complete;}scan;struct{uint8_t mac[6];}regulatory;};
struct a04_ndis_runtime{unsigned lock,next_cancelled,ps_activity;PNDIS_OID_REQUEST next_request;struct{struct a04_wmt_bridge*bridge;uint32_t cancelled_scan_sequences[8];int failed;}radio;};
static unsigned calls,indications,observed_status;static int submit_result;
static int a04_wlan_equal(const void*a,const void*b,unsigned n){return !memcmp(a,b,n);}
static int a04_wmt_bridge_scan_cancel_submit(struct a04_wmt_bridge*b,unsigned sequence,unsigned bss){assert(b&&sequence==255&&bss==3);calls++;return submit_result;}
static void a04_ndis_status(struct a04_ndis_runtime*a,unsigned status,void*p,unsigned n){assert(a&&status==1&&n==sizeof(unsigned));observed_status=*(unsigned*)p;indications++;}
'''.replace('\n+','\n')
suffix=r'''
int main(void){struct a04_wmt_bridge b={.scan={255,3,1,0}};struct a04_ndis_runtime a={.radio.bridge=&b};
 DOT11_RESET_REQUEST input={.dot11ResetType=3};struct q q={.RequestType=12,.DATA.METHOD_INFORMATION={OID_DOT11_RESET_REQUEST,sizeof(input),sizeof(DOT11_STATUS_INDICATION),&input}};a.next_request=&q;
 assert(reset_queued(&a));a.next_cancelled=1;assert(!reset_queued(&a));a.next_cancelled=0;
 input.dot11ResetType=1;assert(!reset_queued(&a));input.dot11ResetType=3;
 q.DATA.METHOD_INFORMATION.InputBufferLength=0;assert(!reset_queued(&a));q.DATA.METHOD_INFORMATION.InputBufferLength=sizeof(input);
 q.DATA.METHOD_INFORMATION.OutputBufferLength=0;assert(!reset_queued(&a));q.DATA.METHOD_INFORMATION.OutputBufferLength=sizeof(DOT11_STATUS_INDICATION);
 input.dot11MacAddress[0]=2;assert(!reset_queued(&a));input.dot11MacAddress[0]=0;
 assert(scan_cancel_for_reset(&a,&b)==NDIS_STATUS_PENDING&&calls==1&&indications==1);
 assert(observed_status==NDIS_STATUS_REQUEST_ABORTED&&!b.scan.pending&&!b.scan.complete&&!a.radio.failed);
 assert(a.radio.cancelled_scan_sequences[7]==0x80000000u);
 b.scan.pending=1;memset(a.radio.cancelled_scan_sequences,0,sizeof(a.radio.cancelled_scan_sequences));submit_result=-9;
 assert(scan_cancel_for_reset(&a,&b)==NDIS_STATUS_PENDING&&calls==2&&indications==2);
 assert(observed_status==NDIS_STATUS_FAILURE&&b.scan.pending&&a.radio.failed&&!a.radio.cancelled_scan_sequences[7]);return 0;}
'''.replace('\n+','\n')
args.output_dir.mkdir(parents=True,exist_ok=True);c=args.output_dir/'scan-owner-host.c';exe=args.output_dir/'scan-owner-host';c.write_text(prefix+body+suffix)
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(c),'-o',str(exe)],check=True);subprocess.run([str(exe.resolve())],check=True)
print('PASS actual queued RESET/cancel: validation, cancellation-not-success, old-sequence retirement, failure retained; mocked NDIS/HIF')
