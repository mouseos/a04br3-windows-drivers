/* SPDX-License-Identifier: MIT */
#include "broker_wdf.h"
#include "broker_wire.h"
int a04_broker_wdf_receive(struct a04_broker_endpoint *e,WDFWAITLOCK lock,WDFREQUEST request,ULONG code)
{
    uint8_t *in,*out;
    size_t in_size,out_size;
    uint8_t bytes[A04_BROKER_WIRE_BYTES];
    struct a04_broker_request q;
    struct a04_broker_response r;
    uint64_t now,ticks;
    LONGLONG timeout;
    NTSTATUS status=STATUS_INVALID_PARAMETER;
    int owner_failed=0;
    if (!request) return 0;
    if (!e || !lock || code!=A04_BROKER_INTERNAL_IOCTL) goto fail;
    status=WdfRequestRetrieveInputBuffer(request,A04_BROKER_WIRE_BYTES,(PVOID *)&in,&in_size);
    if (!NT_SUCCESS(status)) goto fail;
    if (in_size!=A04_BROKER_WIRE_BYTES) {status=STATUS_INVALID_BUFFER_SIZE;goto fail;}
    /* Copy before retrieving output: METHOD_BUFFERED may alias input/output. */
    RtlCopyMemory(bytes,in,sizeof(bytes));
    if (a04_broker_wire_request_decode(&q,bytes,sizeof(bytes))) {status=STATUS_INVALID_PARAMETER;goto fail;}
    status=WdfRequestRetrieveOutputBuffer(request,A04_BROKER_WIRE_BYTES,(PVOID *)&out,&out_size);
    if (!NT_SUCCESS(status)) goto fail;
    if (out_size!=A04_BROKER_WIRE_BYTES) {status=STATUS_INVALID_BUFFER_SIZE;goto fail;}
    now=KeQueryInterruptTime()*100u;
    if (now>=q.deadline) {status=STATUS_IO_TIMEOUT;goto fail;}
    ticks=(q.deadline-now)/100u;
    if (!ticks || ticks>0x7fffffffffffffffULL) {status=STATUS_IO_TIMEOUT;goto fail;}
    timeout=-(LONGLONG)ticks;
    status=WdfWaitLockAcquire(lock,&timeout);
    if (status!=STATUS_SUCCESS) {status=STATUS_IO_TIMEOUT;goto fail;}
    if (a04_broker_endpoint_dispatch(e,&q,&r)) {
        WdfWaitLockRelease(lock);status=STATUS_DEVICE_NOT_READY;goto fail;
    }
    owner_failed=(r.result==A04_BROKER_STOPPED);
    if (a04_broker_wire_response_encode(bytes,sizeof(bytes),&r)) {
        WdfWaitLockRelease(lock);status=STATUS_DEVICE_NOT_READY;goto fail;
    }
    WdfWaitLockRelease(lock);
    RtlCopyMemory(out,bytes,sizeof(bytes));
    WdfRequestCompleteWithInformation(request,STATUS_SUCCESS,sizeof(bytes));return owner_failed;
fail:
    WdfRequestCompleteWithInformation(request,status,0);
    return owner_failed;
}
