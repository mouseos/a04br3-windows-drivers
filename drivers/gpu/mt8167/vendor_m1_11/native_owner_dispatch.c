/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "native_owner_dispatch.h"
static BOOLEAN valid(const struct a04_native_owner_dispatch *d)
{
    return d && d->native && d->start && d->irq && d->synchronize &&
        d->epoch && d->irq->epoch==d->epoch && d->irq->powered &&
        !d->irq->closed && !d->irq->faulted;
}
NTSTATUS a04_native_dispatch_initialize(struct a04_native_owner_dispatch *d,
    struct a04_vendor_native_cold *n,struct a04_vendor_start *s,
    struct a04_native_irq_core *irq,uint64_t epoch,void *ctx,a04_native_synchronize sync)
{
    if(!d || d->state || !n || !s || !irq || !epoch || !sync ||
       KeGetCurrentIrql()!=PASSIVE_LEVEL || irq->epoch!=epoch ||
       !irq->powered || irq->closed || irq->faulted)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(d,sizeof(*d));d->native=n;d->start=s;d->irq=irq;
    d->epoch=epoch;d->sync_context=ctx;d->synchronize=sync;
    d->state=A04_NATIVE_DISPATCH_READY;return STATUS_SUCCESS;
}
NTSTATUS a04_native_dispatch_submit(struct a04_native_owner_dispatch *d,
    uint64_t job,uint32_t generation,uint32_t write)
{
    NTSTATUS status;
    if(!valid(d) || KeGetCurrentIrql()<DISPATCH_LEVEL || !job || !generation ||
       d->state!=A04_NATIVE_DISPATCH_READY || d->irq->masked)return STATUS_INVALID_DEVICE_STATE;
    status=a04_vendor_native_owner_submit(d->native,d->start,write);
    if(!NT_SUCCESS(status))return status;
    d->job=job;d->generation=generation;d->state=A04_NATIVE_DISPATCH_SUBMITTED;
    return STATUS_SUCCESS;
}
BOOLEAN a04_native_dispatch_interrupt(struct a04_native_owner_dispatch *d,
    struct a04_native_owner_event *out)
{
    if(!out)return FALSE;
    RtlZeroMemory(out,sizeof(*out));
    if(!valid(d) || KeGetCurrentIrql()<DISPATCH_LEVEL)return FALSE;
    if(!a04_vendor_native_owner_interrupt_event(d->native,d->start,d->irq,out))return FALSE;
    if(!NT_SUCCESS(out->status)){
        d->failure=out->status;d->state=A04_NATIVE_DISPATCH_FAULT;
    }else if(out->completed){
        if(d->state!=A04_NATIVE_DISPATCH_ARMED || out->epoch!=d->epoch ||
           out->job!=d->job || out->generation!=d->generation){
            out->completed=FALSE;out->epoch=out->job=0;out->generation=0;
            out->status=d->failure=STATUS_INVALID_DEVICE_STATE;d->state=A04_NATIVE_DISPATCH_FAULT;
        }else d->state=A04_NATIVE_DISPATCH_COMPLETE;
    }
    return TRUE;
}
struct service_call {struct a04_native_owner_dispatch *d;NTSTATUS status;BOOLEAN finish,invoked;};
static BOOLEAN begin_service(void *argument)
{
    struct service_call *c=argument;struct a04_native_owner_dispatch *d=c->d;
    c->invoked=TRUE;c->status=STATUS_INVALID_DEVICE_STATE;
    if(!valid(d) || KeGetCurrentIrql()<DISPATCH_LEVEL)return TRUE;
    if(!d->irq->masked && (d->state==A04_NATIVE_DISPATCH_READY || d->state==A04_NATIVE_DISPATCH_COMPLETE)){
        /* Old queued PASSIVE request after graph replacement, or after the
         * ISR already consumed/rearmed completion. No source or DMA result
         * is fabricated; nothing needs servicing in this hardware epoch. */
        c->status=STATUS_SUCCESS;return TRUE;
    }
    if(!d->irq->masked || !d->irq->serial)return TRUE; /* real claim required */
    if(d->state==A04_NATIVE_DISPATCH_SUBMITTED){
        d->state=A04_NATIVE_DISPATCH_FINISHING;c->finish=TRUE;c->status=STATUS_SUCCESS;
    }else if(d->state==A04_NATIVE_DISPATCH_ARMED || d->state==A04_NATIVE_DISPATCH_COMPLETE){
        c->status=a04_native_irq_rearm(d->irq,d->epoch,d->irq->serial);
    }
    return TRUE;
}
static BOOLEAN arm_service(void *argument)
{
    struct service_call *c=argument;struct a04_native_owner_dispatch *d=c->d;
    c->invoked=TRUE;
    if(!valid(d) || KeGetCurrentIrql()<DISPATCH_LEVEL ||
       d->state!=A04_NATIVE_DISPATCH_FINISHING){c->status=STATUS_INVALID_DEVICE_STATE;return TRUE;}
    if(!NT_SUCCESS(c->status)){d->failure=c->status;d->state=A04_NATIVE_DISPATCH_FAULT;return TRUE;}
    c->status=a04_vendor_native_owner_arm_visibility(d->native,d->start,d->irq,
        d->epoch,d->job,d->generation);
    if(NT_SUCCESS(c->status))d->state=A04_NATIVE_DISPATCH_ARMED;
    /* Busy leaves FINISHING with actual cache proof; retry only arm, not job. */
    else if(c->status!=STATUS_DEVICE_BUSY){d->failure=c->status;d->state=A04_NATIVE_DISPATCH_FAULT;}
    return TRUE;
}
static BOOLEAN retry_arm(void *argument)
{
    struct service_call *c=argument;
    if(c->d->state==A04_NATIVE_DISPATCH_FINISHING)return arm_service(argument);
    return begin_service(argument);
}
NTSTATUS a04_native_dispatch_service(struct a04_native_owner_dispatch *d)
{
    struct service_call c={d,STATUS_SUCCESS,FALSE,FALSE};
    if(!d || !d->synchronize || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    /* All state inspection/mutation shared with ISR is under its real lock.
     * Outer worker/lifecycle serialization forbids concurrent service calls. */
    if(!d->synchronize(d->sync_context,retry_arm,&c) || !c.invoked)return STATUS_IO_DEVICE_ERROR;
    if(!NT_SUCCESS(c.status) || !c.finish)return c.status;
    c.status=a04_vendor_native_owner_finish_passive(d->native,d->start,&d->result);
    c.invoked=FALSE;
    if(!d->synchronize(d->sync_context,arm_service,&c) || !c.invoked)return STATUS_IO_DEVICE_ERROR;
    return c.status;
}
