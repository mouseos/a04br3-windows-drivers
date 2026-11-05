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
typedef uintptr_t ULONG_PTR; typedef void *WDFIOTARGET; typedef void *WDFWAITLOCK; typedef void *WDFREQUEST; typedef void *WDFDEVICE; typedef void *WDFQUEUE; typedef void *PVOID;
typedef struct { void *p; size_t n; } WDF_MEMORY_DESCRIPTOR;
typedef struct { int flags; LONGLONG timeout; } WDF_REQUEST_SEND_OPTIONS;
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER (-1)
#define STATUS_INVALID_BUFFER_SIZE (-2)
#define STATUS_IO_TIMEOUT (-3)
#define STATUS_DEVICE_NOT_READY (-4)
#define FILE_DEVICE_UNKNOWN 0x22
#define METHOD_BUFFERED 0
#define FILE_ANY_ACCESS 0
#define CTL_CODE(d,f,m,a) (((d)<<16)|((a)<<14)|((f)<<2)|(m))
#define NT_SUCCESS(x) ((x)>=0)
#define UNREFERENCED_PARAMETER(x) ((void)(x))
#define RtlZeroMemory(p,n) memset(p,0,n)
#define RtlCopyMemory(d,s,n) memcpy(d,s,n)
#define WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(d,b,s) do {(d)->p=(b);(d)->n=(s);}while(0)
#define WDF_REQUEST_SEND_OPTIONS_INIT(o,f) do {(o)->flags=(f);}while(0)
#define WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(o,t) do {(o)->timeout=(t);}while(0)
#define WDF_REQUEST_SEND_OPTION_TIMEOUT 1
uint64_t KeQueryInterruptTime(void);
NTSTATUS WdfWaitLockAcquire(WDFWAITLOCK,LONGLONG *);
void WdfWaitLockRelease(WDFWAITLOCK);
NTSTATUS WdfIoTargetSendInternalIoctlSynchronously(WDFIOTARGET,void *,ULONG,WDF_MEMORY_DESCRIPTOR *,WDF_MEMORY_DESCRIPTOR *,WDF_REQUEST_SEND_OPTIONS *,ULONG_PTR *);
NTSTATUS WdfRequestRetrieveInputBuffer(WDFREQUEST,size_t,PVOID *,size_t *);
NTSTATUS WdfRequestRetrieveOutputBuffer(WDFREQUEST,size_t,PVOID *,size_t *);
void WdfRequestCompleteWithInformation(WDFREQUEST,NTSTATUS,ULONG_PTR);
#endif
'''
harness = r'''
#include <assert.h>
#include "broker_wdf.h"
#include "broker_wire.h"
static uint64_t ticks;static int mode,locks,sends;static LONGLONG lock_timeout,io_timeout;
struct fake_request { uint8_t buffer[48];size_t in_size,out_size;NTSTATUS status;ULONG_PTR information;int completed; };
uint64_t KeQueryInterruptTime(void){return ticks;}
NTSTATUS WdfWaitLockAcquire(WDFWAITLOCK l,LONGLONG *t){assert(l==(void*)2||l==(void*)3);if(t)lock_timeout=*t;if(mode==1)return 0x102;++locks;return 0;}
void WdfWaitLockRelease(WDFWAITLOCK l){assert((l==(void*)2||l==(void*)3)&&locks>0);--locks;}
NTSTATUS WdfIoTargetSendInternalIoctlSynchronously(WDFIOTARGET t,void *request,ULONG code,WDF_MEMORY_DESCRIPTOR *in,WDF_MEMORY_DESCRIPTOR *out,WDF_REQUEST_SEND_OPTIONS *opts,ULONG_PTR *ret){
 struct a04_broker_request q;struct a04_broker_response r={0};
 assert(t==(void*)1&&!request&&code==0x1234&&locks==1&&opts->flags==1);++sends;io_timeout=opts->timeout;
 if(mode==6){struct fake_request f;struct a04_broker_endpoint endpoint={0};memset(&f,0,sizeof(f));f.in_size=f.out_size=48;memcpy(f.buffer,in->p,48);
  a04_broker_wdf_receive(&endpoint,(void*)3,&f,A04_BROKER_INTERNAL_IOCTL);if(f.status)return f.status;memcpy(out->p,f.buffer,48);*ret=f.information;return 0;}
 assert(!a04_broker_wire_request_decode(&q,in->p,(unsigned)in->n));
 r.epoch=q.epoch;r.sequence=q.sequence;r.result=A04_BROKER_OK;r.sample.voltage_mv=4000;r.sample.indicator_tenths_percent=525;r.sample.percent=50;
 if(mode==2)++r.sequence;if(mode==3)ticks=q.deadline/100;
 assert(!a04_broker_wire_response_encode(out->p,(unsigned)out->n,&r));*ret=mode==4?47:48;return mode==5?0x102:0;
}
NTSTATUS WdfRequestRetrieveInputBuffer(WDFREQUEST p,size_t minimum,PVOID *out,size_t *n){struct fake_request *f=p;if(f->in_size<minimum)return -1;*out=f->buffer;*n=f->in_size;return 0;}
NTSTATUS WdfRequestRetrieveOutputBuffer(WDFREQUEST p,size_t minimum,PVOID *out,size_t *n){struct fake_request *f=p;if(f->out_size<minimum)return -1;*out=f->buffer;*n=f->out_size;return 0;}
void WdfRequestCompleteWithInformation(WDFREQUEST p,NTSTATUS s,ULONG_PTR n){struct fake_request *f=p;f->status=s;f->information=n;++f->completed;}
int a04_broker_endpoint_dispatch(struct a04_broker_endpoint *e,const struct a04_broker_request *q,struct a04_broker_response *r){
 (void)e;assert(locks==1||locks==2);++sends;if(q->epoch!=9 || q->sequence!=1)return -1;
 memset(r,0,sizeof(*r));r->epoch=q->epoch;r->sequence=q->sequence;r->result=A04_BROKER_OK;r->sample.voltage_mv=4000;r->sample.indicator_tenths_percent=525;r->sample.percent=50;return 0;
}
int main(void){struct a04_broker_wdf w;struct a04_gauge_sample out,old;int i;
 for(i=0;i<=5;++i){memset(&w,0,sizeof(w));memset(&out,0x55,sizeof(out));old=out;mode=i;locks=0;sends=0;ticks=100;
  assert(!a04_broker_wdf_init(&w,(void*)1,(void*)2,0x1234,9));
  assert(a04_broker_wdf_init(&w,(void*)3,(void*)2,0x1234,9));assert(w.target==(void*)1);
  if(!i){assert(!a04_broker_wdf_read(&w,100000,&out));assert(out.voltage_mv==4000&&sends==1&&lock_timeout==-900&&io_timeout==-900);}
  else {assert(a04_broker_wdf_read(&w,100000,&out));assert(!memcmp(&out,&old,sizeof(out)));if(i==1)assert(!sends&&!w.client.stopped);else assert(w.client.stopped&&sends==1);}
  assert(!locks);
 }
 memset(&w,0,sizeof(w));mode=6;locks=0;sends=0;ticks=100;
 assert(!a04_broker_wdf_init(&w,(void*)1,(void*)2,0x1234,9));
 assert(!a04_broker_wdf_read(&w,100000,&out));assert(out.voltage_mv==4000&&sends==2&&!locks);
 {struct fake_request f;struct a04_broker_request q={9,1,100000};struct a04_broker_response r;struct a04_broker_endpoint e={0};
  memset(&f,0,sizeof(f));f.in_size=f.out_size=48;ticks=100;mode=0;locks=0;sends=0;
  assert(!a04_broker_wire_request_encode(f.buffer,48,&q));
  a04_broker_wdf_receive(&e,(void*)2,&f,A04_BROKER_INTERNAL_IOCTL);
  assert(f.completed==1&&f.status==0&&f.information==48&&sends==1&&!locks);
  assert(!a04_broker_wire_response_decode(&r,f.buffer,48)&&r.sample.voltage_mv==4000);
  memset(&f,0,sizeof(f));f.in_size=f.out_size=48;assert(!a04_broker_wire_request_encode(f.buffer,48,&q));f.buffer[32]=1;sends=0;
  a04_broker_wdf_receive(&e,(void*)2,&f,A04_BROKER_INTERNAL_IOCTL);assert(f.completed==1&&f.status!=0&&!f.information&&!sends);
  memset(&f,0,sizeof(f));f.in_size=f.out_size=48;assert(!a04_broker_wire_request_encode(f.buffer,48,&q));mode=1;
  a04_broker_wdf_receive(&e,(void*)2,&f,A04_BROKER_INTERNAL_IOCTL);assert(f.status==STATUS_IO_TIMEOUT&&!f.information&&!sends);
  memset(&f,0,sizeof(f));f.in_size=f.out_size=48;assert(!a04_broker_wire_request_encode(f.buffer,48,&q));mode=0;ticks=1000;
  a04_broker_wdf_receive(&e,(void*)2,&f,A04_BROKER_INTERNAL_IOCTL);assert(f.status==STATUS_IO_TIMEOUT&&!sends);
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
                    '-I'+str(p),'-I'+str(src),str(src/'broker_wdf.c'),str(src/'broker_wdf_receiver.c'),str(src/'broker_client.c'),
                    str(src/'broker_wire.c'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('BQ broker WDF timeout, lock and wire mock: PASS')
