#!/usr/bin/env python3
"""Compile actual WDF receiver queue code against a controlled host stub."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
header=r'''
#ifndef MOCK_WDF
#define MOCK_WDF
#include <stdint.h>
#include <stddef.h>
typedef int32_t NTSTATUS;typedef uint32_t ULONG;typedef void *WDFDEVICE;typedef void *WDFQUEUE;typedef void *WDFREQUEST;typedef void *WDFWAITLOCK;typedef void *WDFIOTARGET;typedef void VOID;
#define STATUS_INVALID_PARAMETER (-1)
#define STATUS_SUCCESS 0
#define NT_SUCCESS(s) ((s)>=0)
#define FILE_DEVICE_UNKNOWN 0x22
#define METHOD_BUFFERED 0
#define FILE_ANY_ACCESS 0
#define CTL_CODE(d,f,m,a) (((d)<<16)|((a)<<14)|((f)<<2)|(m))
#define UNREFERENCED_PARAMETER(x) ((void)(x))
#define WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(t,n) t *n(WDFQUEUE q);
typedef void EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL(WDFQUEUE,WDFREQUEST,size_t,size_t,ULONG);
#define WdfIoQueueDispatchSequential 1
#define WdfExecutionLevelPassive 2
#define WdfRequestTypeDeviceControlInternal 3
typedef void (*callback)(WDFQUEUE,WDFREQUEST,size_t,size_t,ULONG);
typedef struct {callback EvtIoInternalDeviceControl;} WDF_IO_QUEUE_CONFIG;
typedef struct {int ExecutionLevel;} WDF_OBJECT_ATTRIBUTES;
#define WDF_IO_QUEUE_CONFIG_INIT(c,m) do {(void)(m);(c)->EvtIoInternalDeviceControl=0;}while(0)
#define WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(a,t) do {(a)->ExecutionLevel=0;}while(0)
NTSTATUS WdfIoQueueCreate(WDFDEVICE,WDF_IO_QUEUE_CONFIG *,WDF_OBJECT_ATTRIBUTES *,WDFQUEUE *);
NTSTATUS WdfDeviceConfigureRequestDispatching(WDFDEVICE,WDFQUEUE,int);
void WdfObjectDelete(WDFQUEUE);
#endif
'''
test=r'''
#include <assert.h>
#include <string.h>
#include "broker_wdf.h"
#include "broker_wdf_queue.c"
static BROKER_QUEUE_CONTEXT saved;static int configured,deleted,received,fail,notify,broker_failure;
BROKER_QUEUE_CONTEXT *BrokerQueueContext(WDFQUEUE q){assert(q==(void*)3);return &saved;}
NTSTATUS WdfIoQueueCreate(WDFDEVICE d,WDF_IO_QUEUE_CONFIG *c,WDF_OBJECT_ATTRIBUTES *a,WDFQUEUE *q){assert(d==(void*)1&&c->EvtIoInternalDeviceControl==BrokerInternalControl&&a->ExecutionLevel==WdfExecutionLevelPassive);if(fail==1)return -8;*q=(void*)3;return 0;}
NTSTATUS WdfDeviceConfigureRequestDispatching(WDFDEVICE d,WDFQUEUE q,int t){assert(d==(void*)1&&q==(void*)3&&t==WdfRequestTypeDeviceControlInternal&&saved.endpoint&&saved.lock==(void*)2);++configured;return fail==2?-9:0;}
void WdfObjectDelete(WDFQUEUE q){assert(q==(void*)3);++deleted;}
int a04_broker_wdf_receive(struct a04_broker_endpoint *e,WDFWAITLOCK l,WDFREQUEST r,ULONG code){assert(e==saved.endpoint&&l==(void*)2&&r==(void*)4&&code==123);++received;return broker_failure;}
static void failed(void *p){assert(p==(void*)5);++notify;}
int main(void){struct a04_broker_endpoint e={0};WDFQUEUE q=(void*)9;int i;
 for(i=0;i<=2;++i){fail=i;configured=deleted=0;memset(&saved,0,sizeof(saved));q=(void*)9;
  if(i){assert(a04_broker_wdf_register_receiver((void*)1,&e,(void*)2,failed,(void*)5,&q)<0);assert(!q);assert(deleted==(i==2));}
  else {assert(!a04_broker_wdf_register_receiver((void*)1,&e,(void*)2,failed,(void*)5,&q));assert(q==(void*)3&&configured==1&&!deleted);BrokerInternalControl(q,(void*)4,0,0,123);assert(received==1&&!notify);broker_failure=1;BrokerInternalControl(q,(void*)4,0,0,123);assert(received==2&&notify==1);}
 }
 return 0;}
'''
(root/'build').mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='bq-queue-',dir=root/'build') as td:
    p=Path(td);(p/'ntddk.h').write_text(header);(p/'wdf.h').write_text('#include <ntddk.h>\n');(p/'test.c').write_text(test)
    src=root/'drivers/power/bq24160'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(p),'-I'+str(src),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('BQ owner WDF queue registration and failure: PASS (host mock)')
