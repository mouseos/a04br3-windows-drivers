#!/usr/bin/env python3
"""Run the real WDF client adapter against a fake lower I/O target."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
mock = r'''
#ifndef MOCK_H
#define MOCK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef int32_t NTSTATUS; typedef int64_t LONGLONG; typedef uint32_t ULONG;
typedef uintptr_t ULONG_PTR; typedef void *WDFIOTARGET; typedef void *WDFWAITLOCK;
typedef struct { void *p; size_t n; } WDF_MEMORY_DESCRIPTOR;
typedef struct { int flags; LONGLONG timeout; } WDF_REQUEST_SEND_OPTIONS;
#define STATUS_SUCCESS 0
#define NT_SUCCESS(x) ((x)>=0)
#define UNREFERENCED_PARAMETER(x) ((void)(x))
#define RtlZeroMemory(p,n) memset(p,0,n)
#define WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(d,b,s) do {(d)->p=(b);(d)->n=(s);}while(0)
#define WDF_REQUEST_SEND_OPTIONS_INIT(o,f) do {(o)->flags=(f);}while(0)
#define WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(o,t) do {(o)->timeout=(t);}while(0)
#define WDF_REQUEST_SEND_OPTION_TIMEOUT 1
uint64_t KeQueryInterruptTime(void);
NTSTATUS WdfWaitLockAcquire(WDFWAITLOCK,LONGLONG *);
void WdfWaitLockRelease(WDFWAITLOCK);
NTSTATUS WdfIoTargetSendInternalIoctlSynchronously(WDFIOTARGET,void *,ULONG,WDF_MEMORY_DESCRIPTOR *,WDF_MEMORY_DESCRIPTOR *,WDF_REQUEST_SEND_OPTIONS *,ULONG_PTR *);
#endif
'''
harness = r'''
#include <assert.h>
#include "broker_wdf.h"
#include "broker_wire.h"
static uint64_t ticks;static int mode,locks,sends;static LONGLONG lock_timeout,io_timeout;
uint64_t KeQueryInterruptTime(void){return ticks;}
NTSTATUS WdfWaitLockAcquire(WDFWAITLOCK l,LONGLONG *t){assert(l==(void*)2);if(t)lock_timeout=*t;if(mode==1)return 0x102;++locks;return 0;}
void WdfWaitLockRelease(WDFWAITLOCK l){assert(l==(void*)2&&locks==1);--locks;}
NTSTATUS WdfIoTargetSendInternalIoctlSynchronously(WDFIOTARGET t,void *request,ULONG code,WDF_MEMORY_DESCRIPTOR *in,WDF_MEMORY_DESCRIPTOR *out,WDF_REQUEST_SEND_OPTIONS *opts,ULONG_PTR *ret){
 struct a04_broker_request q;struct a04_broker_response r={0};
 assert(t==(void*)1&&!request&&code==0x1234&&locks==1&&opts->flags==1);++sends;io_timeout=opts->timeout;
 assert(!a04_broker_wire_request_decode(&q,in->p,(unsigned)in->n));
 r.epoch=q.epoch;r.sequence=q.sequence;r.result=A04_BROKER_OK;r.sample.voltage_mv=4000;r.sample.indicator_tenths_percent=525;r.sample.percent=50;
 if(mode==2)++r.sequence;if(mode==3)ticks=q.deadline/100;
 assert(!a04_broker_wire_response_encode(out->p,(unsigned)out->n,&r));*ret=mode==4?47:48;return mode==5?0x102:0;
}
int main(void){struct a04_broker_wdf w;struct a04_gauge_sample out,old;int i;
 for(i=0;i<=5;++i){memset(&w,0,sizeof(w));memset(&out,0x55,sizeof(out));old=out;mode=i;locks=0;sends=0;ticks=100;
  assert(!a04_broker_wdf_init(&w,(void*)1,(void*)2,0x1234,9));
  assert(a04_broker_wdf_init(&w,(void*)3,(void*)2,0x1234,9));assert(w.target==(void*)1);
  if(!i){assert(!a04_broker_wdf_read(&w,100000,&out));assert(out.voltage_mv==4000&&sends==1&&lock_timeout==-900&&io_timeout==-900);}
  else {assert(a04_broker_wdf_read(&w,100000,&out));assert(!memcmp(&out,&old,sizeof(out)));if(i==1)assert(!sends&&!w.client.stopped);else assert(w.client.stopped&&sends==1);}
  assert(!locks);
 }
 return 0;}
'''
with tempfile.TemporaryDirectory(prefix='bq-wdf-', dir=root / 'build') as td:
    p = Path(td)
    (p/'ntddk.h').write_text(mock)
    (p/'wdf.h').write_text('#include <ntddk.h>\n')
    (p/'test.c').write_text(harness)
    src = root/'drivers/power/bq24160'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
                    '-I'+str(p),'-I'+str(src),str(src/'broker_wdf.c'),str(src/'broker_client.c'),
                    str(src/'broker_wire.c'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('BQ broker WDF timeout, lock and wire mock: PASS')
