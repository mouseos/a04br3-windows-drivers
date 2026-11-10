#!/usr/bin/env python3
"""実OID wrapperを失敗分岐で実行。query buffer/SSID/keyをloggerへ渡さない。"""
from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[1];src=(r/'drivers/network/mt6630/native_control.c').read_text();body=src[src.index('NDIS_STATUS a04_ndis_control('):]
fixture=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "checkpoint_wire.h"
typedef int NDIS_STATUS;typedef unsigned ULONG;
#define NdisRequestSetInformation 1
#define NdisRequestMethod 2
#define NDIS_STATUS_SUCCESS 0
#define NDIS_STATUS_PENDING 0x103
#define OID_DOT11_CONNECT_REQUEST 10
#define OID_DOT11_SCAN_REQUEST 11
#define OID_DOT11_CIPHER_KEY_MAPPING_KEY 12
#define OID_DOT11_CIPHER_DEFAULT_KEY 13
#define OID_DOT11_PORT_STATE_NOTIFICATION 14
struct request {int RequestType;struct{struct{ULONG Oid;}QUERY_INFORMATION,SET_INFORMATION;}DATA;};typedef struct request*PNDIS_OID_REQUEST;
struct a04_ndis_runtime {int connection_cp;uint64_t initialize_generation;ULONG prior_rejected_oid,prior_rejected_status,runtime_last_oid,runtime_last_oid_status;struct{int failed;}radio;};
static NDIS_STATUS answer;static int begins;static unsigned events,last_stage,last_raw,last_detail;
static void a04_cp_connection_begin(int*p,uint64_t parent,ULONG ctx){(void)p;assert(parent==17&&ctx==2);++begins;}
static void a04_ndis_connection_trace(void*p,unsigned stage,unsigned phase,unsigned kind,uint32_t raw,uint32_t detail)
{(void)p;assert(phase>=1&&phase<=3&&kind<=4);++events;last_stage=stage;last_raw=raw;last_detail=detail;}
static void a04_ndis_runtime_snapshot(void*p,ULONG c,ULONG s,ULONG phase){(void)p;(void)c;(void)s;(void)phase;}
static NDIS_STATUS control_impl(void*p,PNDIS_OID_REQUEST q)
{(void)p;/* NdisMOidRequestComplete may release q before return. Poison it. */q->RequestType=99;q->DATA.QUERY_INFORMATION.Oid=0xdeadbeef;return answer;}
''' + body + r'''
int main(void){
 struct a04_ndis_runtime a={.initialize_generation=17};struct request q;
 for(unsigned oid=10;oid<=15;oid++){
  q=(struct request){0};q.RequestType=1;q.DATA.QUERY_INFORMATION.Oid=q.DATA.SET_INFORMATION.Oid=oid;
  answer=(int)0xc00000bb;assert(a04_ndis_control(&a,&q)==answer);assert(a.runtime_last_oid==oid&&a.prior_rejected_oid==oid&&a.prior_rejected_status==(unsigned)answer);
  assert(events>0);if(oid==10)assert(begins==1);
 }
 assert(last_stage==A04_CT_OID&&last_raw==0xc00000bb&&last_detail==15);
 q=(struct request){0};q.RequestType=1;q.DATA.QUERY_INFORMATION.Oid=q.DATA.SET_INFORMATION.Oid=10;answer=0x103;
 assert(a04_ndis_control(&a,&q)==0x103&&begins==2&&last_stage==A04_CT_GATE&&last_raw==0x103);
 puts("PASS actual OID wrapper: preconnect failure, keys/port, unsupported preserved, generation begin, completed-request lifetime");
}
'''
out=r/'build/mt6630-connect-oid-trace';out.mkdir(parents=True,exist_ok=True);(out/'actual.c').write_text(fixture)
subprocess.run(['clang','-std=c11','-O1','-fsanitize=address,undefined','-Wall','-Wextra','-Werror','-I'+str(r/'drivers/network/mt6630'),str(out/'actual.c'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
