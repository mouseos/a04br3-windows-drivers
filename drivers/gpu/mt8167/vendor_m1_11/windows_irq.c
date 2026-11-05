/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 MIPS-wrapper IRQ transport. Register semantics follow m1.11
 * rgxinit.c RGXFWIrqEventRx; deferred request handling is supplied by owner.
 */
#include "windows_irq.h"
struct a04_vendor_irq {
    WDFINTERRUPT interrupt;WDFWORKITEM work;
    volatile UCHAR *core;ULONG bytes;
    a04_vendor_irq_service service;void *context;
    volatile LONG powered,sequence,serviced;
};
typedef struct { struct a04_vendor_irq *owner; } WORK_OBJECT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(struct a04_vendor_irq,IrqObject)
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(WORK_OBJECT,WorkObject)
EVT_WDF_INTERRUPT_ISR a04_vendor_isr;
EVT_WDF_INTERRUPT_DPC a04_vendor_dpc;
EVT_WDF_WORKITEM a04_vendor_work;
BOOLEAN a04_vendor_isr(WDFINTERRUPT interrupt,ULONG message)
{
    struct a04_vendor_irq *q=IrqObject(interrupt);ULONG status;
    UNREFERENCED_PARAMETER(message);
    if(!q || !q->core || !InterlockedCompareExchange(&q->powered,0,0)) return FALSE;
    status=READ_REGISTER_ULONG((volatile ULONG *)(void *)(q->core+0x8a8u));
    if(status==0xffffffffu || !(status&1u)) return FALSE;
    WRITE_REGISTER_ULONG((volatile ULONG *)(void *)(q->core+0x8b0u),1u);
    InterlockedIncrement(&q->sequence);
    WdfInterruptQueueDpcForIsr(interrupt);return TRUE;
}
VOID a04_vendor_dpc(WDFINTERRUPT interrupt,WDFOBJECT device)
{
    struct a04_vendor_irq *q=IrqObject(interrupt);
    UNREFERENCED_PARAMETER(device);
    if(q && q->work) WdfWorkItemEnqueue(q->work);
}
VOID a04_vendor_work(WDFWORKITEM work)
{
    struct a04_vendor_irq *q=WorkObject(work)->owner;LONG sequence;
    if(!q || !q->service || !InterlockedCompareExchange(&q->powered,0,0)) return;
    sequence=InterlockedCompareExchange(&q->sequence,0,0);
    if(sequence==InterlockedCompareExchange(&q->serviced,0,0)) return;
    /* On failure keep the sequence pending; never acknowledge unhandled FWCCB
     * commands. The owner retries after startup, queue service or recovery. */
    if(NT_SUCCESS(q->service(q->context))) {
        InterlockedExchange(&q->serviced,sequence);
        if(sequence!=InterlockedCompareExchange(&q->sequence,0,0)) WdfWorkItemEnqueue(work);
    }
}
NTSTATUS a04_vendor_irq_create(WDFDEVICE device,a04_vendor_irq_service service,
    void *context,struct a04_vendor_irq **out)
{
    WDF_OBJECT_ATTRIBUTES attr;WDF_WORKITEM_CONFIG work_config;WDF_INTERRUPT_CONFIG irq_config;
    struct a04_vendor_irq *q;WDFINTERRUPT interrupt;NTSTATUS status;
    if(!device || !service || !out || *out)
        return STATUS_INVALID_PARAMETER;
    WDF_INTERRUPT_CONFIG_INIT(&irq_config,a04_vendor_isr,a04_vendor_dpc);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,struct a04_vendor_irq);
    status=WdfInterruptCreate(device,&irq_config,&attr,&interrupt);
    if(!NT_SUCCESS(status)) return status;
    q=IrqObject(interrupt);RtlZeroMemory(q,sizeof(*q));q->interrupt=interrupt;
    q->service=service;q->context=context;
    WDF_WORKITEM_CONFIG_INIT(&work_config,a04_vendor_work);
    work_config.AutomaticSerialization=FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,WORK_OBJECT);attr.ParentObject=interrupt;
    status=WdfWorkItemCreate(&work_config,&attr,&q->work);
    if(!NT_SUCCESS(status)) { WdfObjectDelete(interrupt);return status; }
    WorkObject(q->work)->owner=q;
    *out=q;return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_irq_bind(struct a04_vendor_irq *q,volatile UCHAR *core,ULONG bytes)
{
    if(!q || !core || bytes<0x8b4u || ((ULONG_PTR)core&3u) || q->core ||
       InterlockedCompareExchange(&q->powered,0,0)) return STATUS_INVALID_PARAMETER;
    q->core=core;q->bytes=bytes;return STATUS_SUCCESS;
}
NTSTATUS a04_vendor_irq_unbind(struct a04_vendor_irq *q)
{
    if(!q || InterlockedCompareExchange(&q->powered,0,0)) return STATUS_INVALID_DEVICE_STATE;
    q->core=NULL;q->bytes=0;return STATUS_SUCCESS;
}
void a04_vendor_irq_set_power(struct a04_vendor_irq *q,BOOLEAN powered)
{ if(q) InterlockedExchange(&q->powered,powered?1:0); }
void a04_vendor_irq_kick(struct a04_vendor_irq *q)
{ if(q && q->work) WdfWorkItemEnqueue(q->work); }
void a04_vendor_irq_flush(struct a04_vendor_irq *q)
{ if(q && q->work) WdfWorkItemFlush(q->work); }
