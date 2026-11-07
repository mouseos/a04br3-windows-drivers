/* SPDX-License-Identifier: MIT */
#include "broker_wdf.h"
#include "broker_wire.h"
static uint64_t wdf_now(void *context)
{ UNREFERENCED_PARAMETER(context); return KeQueryInterruptTime()*100u; }
static int exchange(void *context,const struct a04_broker_request *q,struct a04_broker_response *r)
{
    struct a04_broker_wdf *w=context;
    uint8_t input[A04_BROKER_WIRE_BYTES], output[A04_BROKER_WIRE_BYTES];
    WDF_MEMORY_DESCRIPTOR in_desc,out_desc;
    WDF_REQUEST_SEND_OPTIONS options;
    ULONG_PTR returned=0;
    uint64_t now, ticks;
    NTSTATUS status;
    if (!w || !q || !r || a04_broker_wire_request_encode(input,sizeof(input),q)) return -1;
    now=wdf_now(w);
    if (now>=q->deadline) return -1;
    ticks=(q->deadline-now)/100u;
    /* Truncate toward an earlier timeout. Never encode an infinite timeout. */
    if (!ticks || ticks>0x7fffffffffffffffULL) return -1;
    RtlZeroMemory(output,sizeof(output));
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&in_desc,input,sizeof(input));
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&out_desc,output,sizeof(output));
    WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_TIMEOUT);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options,-(LONGLONG)ticks);
    status=WdfIoTargetSendInternalIoctlSynchronously(w->target,NULL,w->ioctl,&in_desc,&out_desc,&options,&returned);
    if (status!=STATUS_SUCCESS || returned!=sizeof(output)) return -1;
    return a04_broker_wire_response_decode(r,output,sizeof(output));
}
int a04_broker_wdf_init(struct a04_broker_wdf *w,WDFIOTARGET target,WDFWAITLOCK lock,ULONG ioctl,uint64_t epoch)
{
    struct a04_client_transport t;
    if (!w || !target || !lock || !ioctl || w->target || w->lock ||
        w->client.epoch || w->client.stopped || w->client.entered) return -1;
    w->target=target;w->lock=lock;w->ioctl=ioctl;
    t.context=w;t.now_ns=wdf_now;t.exchange=exchange;
    return a04_broker_client_init(&w->client,&t,epoch,wdf_now(w));
}
int a04_broker_wdf_read(struct a04_broker_wdf *w,uint64_t deadline,struct a04_gauge_sample *out)
{
    int result;
    uint64_t now,ticks;
    LONGLONG timeout;
    if (!w || !w->lock || !out) return -1;
    now=wdf_now(w);if(now>=deadline) return -1;
    ticks=(deadline-now)/100u;if(!ticks || ticks>0x7fffffffffffffffULL) return -1;
    timeout=-(LONGLONG)ticks;
    /* STATUS_TIMEOUT is informational, so NT_SUCCESS alone is insufficient. */
    if (WdfWaitLockAcquire(w->lock,&timeout)!=STATUS_SUCCESS) return -1;
    result=a04_broker_client_read(&w->client,deadline,out);
    WdfWaitLockRelease(w->lock);
    return result;
}
void a04_broker_wdf_stop(struct a04_broker_wdf *w)
{
    if (!w || !w->lock || WdfWaitLockAcquire(w->lock,NULL)!=STATUS_SUCCESS) return;
    a04_broker_client_stop(&w->client);
    WdfWaitLockRelease(w->lock);
}
