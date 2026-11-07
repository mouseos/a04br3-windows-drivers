/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * One dedicated SD hardware thread; bounded cancel-safe queue. A single short
 * completion work item invokes upper completions outside the hardware thread,
 * so an inline upper power transition cannot join the thread completing it.
 * No shared worker performs PIO, waits for serial, or drains hardware. */
#include <ntifs.h>
#include "windows.h"
static IO_CSQ_INSERT_IRP_EX insert;
static IO_CSQ_REMOVE_IRP remove_irp;
static IO_CSQ_PEEK_NEXT_IRP peek;
static IO_CSQ_ACQUIRE_LOCK acquire;
static IO_CSQ_RELEASE_LOCK release;
static IO_CSQ_COMPLETE_CANCELED_IRP cancelled;
static struct sd_host *host(PIO_CSQ csq){return CONTAINING_RECORD(csq,struct sd_host,csq);}
static VOID acquire(PIO_CSQ csq,PKIRQL irql){KeAcquireSpinLock(&host(csq)->queue_lock,irql);}
static VOID release(PIO_CSQ csq,KIRQL irql){KeReleaseSpinLock(&host(csq)->queue_lock,irql);}
static NTSTATUS insert(PIO_CSQ csq,PIRP irp,PVOID context)
{
 struct sd_host *h=host(csq);struct sd_work *w=irp->Tail.Overlay.DriverContext[0];UNREFERENCED_PARAMETER(context);
 /* Admission/rundown is reserved before IoCsqInsertIrpEx: an IRP already
  * cancelled at insertion can be completed without this callback running. */
 if(h->closing||!h->consumer_online||h->consumer_stop)return STATUS_DEVICE_NOT_READY;
 InsertTailList(&h->pending,&w->link);return STATUS_SUCCESS;
}
static VOID remove_irp(PIO_CSQ csq,PIRP irp)
{UNREFERENCED_PARAMETER(csq);RemoveEntryList(&((struct sd_work *)irp->Tail.Overlay.DriverContext[0])->link);}
static PIRP peek(PIO_CSQ csq,PIRP irp,PVOID context)
{
 struct sd_host *h=host(csq);PLIST_ENTRY next;UNREFERENCED_PARAMETER(context);
 next=irp?((struct sd_work *)irp->Tail.Overlay.DriverContext[0])->link.Flink:h->pending.Flink;
 return next==&h->pending?NULL:CONTAINING_RECORD(next,struct sd_work,link)->irp;
}
static VOID cancelled(PIO_CSQ csq,PIRP irp)
{UNREFERENCED_PARAMETER(csq);SdFinish(irp->Tail.Overlay.DriverContext[0],STATUS_CANCELLED,0);}
/* All SRB/MDL-facing writes finish before releasing hardware rundown. */
static void set_result(PIRP irp,NTSTATUS status,ULONG_PTR information)
{
 if(!NT_SUCCESS(status)){
  PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(irp);
  information=0;
  if(stack->MajorFunction==IRP_MJ_SCSI&&stack->Parameters.Scsi.Srb&&stack->Parameters.Scsi.Srb->Length>=sizeof(SCSI_REQUEST_BLOCK)){
   PSCSI_REQUEST_BLOCK s=stack->Parameters.Scsi.Srb;s->DataTransferLength=0;
   if(status==STATUS_CANCELLED){s->SrbStatus=SRB_STATUS_ABORTED;s->ScsiStatus=0;}
   else if(s->SrbStatus==SRB_STATUS_PENDING||s->SrbStatus==SRB_STATUS_SUCCESS)
    s->SrbStatus=status==STATUS_CANCELLED?SRB_STATUS_ABORTED:status==STATUS_IO_TIMEOUT?SRB_STATUS_TIMEOUT:
     status==STATUS_DEVICE_BUSY||status==STATUS_INSUFFICIENT_RESOURCES?SRB_STATUS_BUSY:SRB_STATUS_NO_DEVICE;
  }
 }
 irp->IoStatus.Status=status;irp->IoStatus.Information=information;
}
static void drop_work(struct sd_work *w)
{
 if(!InterlockedDecrement(&w->refs)){
  WdfObjectDereference(w->child);WdfObjectDereference(w->host->device);ExFreePoolWithTag(w,SD_TAG);
 }
}
static VOID complete(PDEVICE_OBJECT device,PVOID context)
{
 struct sd_host *h=context;UNREFERENCED_PARAMETER(device);
 for(;;){
  KIRQL irql;struct sd_work *w;KeAcquireSpinLock(&h->queue_lock,&irql);
  if(IsListEmpty(&h->completions)){
   h->completion_scheduled=FALSE;KeReleaseSpinLock(&h->queue_lock,irql);
   WdfObjectDereference(h->device);return; /* scheduling reference, last use */
  }
  w=CONTAINING_RECORD(RemoveHeadList(&h->completions),struct sd_work,link);KeReleaseSpinLock(&h->queue_lock,irql);
  /* No serial/MMIO/PIO access, and no wait for the dedicated thread here. */
  w->irp->Tail.Overlay.DriverContext[0]=w->saved_context;IoCompleteRequest(w->irp,IO_DISK_INCREMENT);
  InterlockedDecrement(&h->inflight);drop_work(w);
 }
}
void SdFinish(struct sd_work *w,NTSTATUS status,ULONG_PTR information)
{
 struct sd_host *h=w->host;KIRQL irql;BOOLEAN schedule=FALSE;
 /* An already-cancelled insertion may complete synchronously. The inserter
  * keeps a separate work reference until the CSQ call returns; finish wins
  * only once, including an insertion refusal racing cancellation. */
 if(InterlockedCompareExchange(&w->finished,1,0))return;
 set_result(w->irp,status,information);
 KeAcquireSpinLock(&h->queue_lock,&irql);
 ExReleaseRundownProtection(&h->requests);
 if(--h->hardware_users==0)KeSetEvent(&h->hardware_idle,IO_NO_INCREMENT,FALSE);
 InsertTailList(&h->completions,&w->link);
 if(!h->completion_scheduled){h->completion_scheduled=TRUE;WdfObjectReference(h->device);schedule=TRUE;}
 KeReleaseSpinLock(&h->queue_lock,irql);
 if(schedule)IoQueueWorkItem(h->completion_item,complete,DelayedWorkQueue,h);
}
static VOID consumer(PVOID context)
{
 struct sd_host *h=context;
 for(;;){
  PIRP irp=IoCsqRemoveNextIrp(&h->csq,NULL);KIRQL irql;BOOLEAN stop;
  if(irp){SdExecute(irp->Tail.Overlay.DriverContext[0]);continue;}
  KeAcquireSpinLock(&h->queue_lock,&irql);stop=h->consumer_stop&&IsListEmpty(&h->pending);KeReleaseSpinLock(&h->queue_lock,irql);
  if(stop)break;
  (void)KeWaitForSingleObject(&h->consumer_wake,Executive,KernelMode,FALSE,NULL);
 }
 WdfObjectDereference(h->device);PsTerminateSystemThread(STATUS_SUCCESS);
}
NTSTATUS SdConsumerInitialize(struct sd_host *h)
{
 KeInitializeSpinLock(&h->queue_lock);InitializeListHead(&h->pending);InitializeListHead(&h->completions);
 KeInitializeEvent(&h->consumer_wake,SynchronizationEvent,FALSE);KeInitializeEvent(&h->hardware_idle,NotificationEvent,TRUE);
 h->consumer_stop=TRUE;
 return IoCsqInitializeEx(&h->csq,insert,remove_irp,peek,acquire,release,cancelled);
}
NTSTATUS SdConsumerStart(struct sd_host *h)
{
 OBJECT_ATTRIBUTES a;NTSTATUS status;KIRQL irql;
 if(h->consumer_thread||h->hardware_users)return STATUS_INVALID_DEVICE_STATE;
 if(!h->completion_item){h->completion_item=IoAllocateWorkItem(WdfDeviceWdmGetDeviceObject(h->device));if(!h->completion_item)return STATUS_INSUFFICIENT_RESOURCES;}
 KeAcquireSpinLock(&h->queue_lock,&irql);
 if(h->surprised||h->supplier_lost){KeReleaseSpinLock(&h->queue_lock,irql);return STATUS_DEVICE_NOT_CONNECTED;}
 h->consumer_stop=FALSE;KeReleaseSpinLock(&h->queue_lock,irql);
 InitializeObjectAttributes(&a,NULL,OBJ_KERNEL_HANDLE,NULL,NULL);WdfObjectReference(h->device);
 status=PsCreateSystemThread(&h->consumer_thread,THREAD_ALL_ACCESS,&a,NULL,NULL,consumer,h);
 if(!NT_SUCCESS(status)){
  h->consumer_thread=NULL;WdfObjectDereference(h->device);
  KeAcquireSpinLock(&h->queue_lock,&irql);h->consumer_stop=TRUE;KeReleaseSpinLock(&h->queue_lock,irql);return status;
 }
 KeAcquireSpinLock(&h->queue_lock,&irql);
 /* A removal/lost-supplier callback may have closed admission while the
  * thread was being created. Never reopen over that terminal notification. */
 if(h->consumer_stop||h->surprised||h->supplier_lost){
  KeReleaseSpinLock(&h->queue_lock,irql);KeSetEvent(&h->consumer_wake,IO_NO_INCREMENT,FALSE);return STATUS_DEVICE_NOT_CONNECTED;
 }
 h->consumer_online=TRUE;InterlockedExchange(&h->closing,0);KeReleaseSpinLock(&h->queue_lock,irql);
 return STATUS_SUCCESS;
}
void SdStopAdmission(struct sd_host *h)
{
 KIRQL irql;KeAcquireSpinLock(&h->queue_lock,&irql);
 InterlockedExchange(&h->closing,1);h->consumer_online=FALSE;h->consumer_stop=TRUE;
 KeReleaseSpinLock(&h->queue_lock,irql);KeSetEvent(&h->consumer_wake,IO_NO_INCREMENT,FALSE);
}
NTSTATUS SdConsumerStop(struct sd_host *h)
{
 LARGE_INTEGER timeout;NTSTATUS status;
 SdStopAdmission(h);if(!h->consumer_thread)return h->hardware_users?STATUS_DEVICE_BUSY:STATUS_SUCCESS;
 /* The normal I/O budget is <=10s including queue residence. Cancellation
  * and closing revoke PIO admission sooner. A stuck lower supplier is not
  * forced complete: timeout retains all mappings, thread and power lease. */
 timeout.QuadPart=-120000000;status=KeWaitForSingleObject(&h->hardware_idle,Executive,KernelMode,FALSE,&timeout);
 if(status!=STATUS_SUCCESS)return STATUS_IO_TIMEOUT;
 /* Gate closed + hardware_users=0 => no remaining rundown holder. */
 ExWaitForRundownProtectionRelease(&h->requests);
 timeout.QuadPart=-20000000;status=ZwWaitForSingleObject(h->consumer_thread,FALSE,&timeout);
 if(status!=STATUS_SUCCESS)return STATUS_IO_TIMEOUT;
 ZwClose(h->consumer_thread);h->consumer_thread=NULL;return STATUS_SUCCESS;
}
VOID SdConsumerDestroy(WDFOBJECT object)
{
 struct sd_host *h=SdHost((WDFDEVICE)object);
 /* Thread and scheduled completion references keep this object alive until
  * their final use. Normal PnP stop already joined the thread. */
 if(h->consumer_thread)ZwClose(h->consumer_thread);
 if(h->completion_item)IoFreeWorkItem(h->completion_item);
}
NTSTATUS SdSubmit(WDFDEVICE child,PIRP irp)
{
 struct sd_host *h=SdChild(child)->host;struct sd_work *w;NTSTATUS status;ULONG seconds=5;KIRQL irql;
 uint64_t received=KeQueryInterruptTime();PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(irp);
 if(stack->MajorFunction==IRP_MJ_SCSI&&stack->Parameters.Scsi.Srb&&stack->Parameters.Scsi.Srb->Length>=sizeof(SCSI_REQUEST_BLOCK)&&stack->Parameters.Scsi.Srb->TimeOutValue)seconds=stack->Parameters.Scsi.Srb->TimeOutValue;
 if(seconds>10)seconds=10;
 w=ExAllocatePoolWithTag(NonPagedPool,sizeof(*w),SD_TAG);
 if(!w){set_result(irp,STATUS_INSUFFICIENT_RESOURCES,0);IoCompleteRequest(irp,IO_NO_INCREMENT);return STATUS_INSUFFICIENT_RESOURCES;}
 RtlZeroMemory(w,sizeof(*w));w->refs=1;w->host=h;w->child=child;w->irp=irp;w->generation=InterlockedCompareExchange(&h->generation,0,0);
 w->deadline=received+(uint64_t)seconds*10000000u;
 WdfObjectReference(child);WdfObjectReference(h->device);
 KeAcquireSpinLock(&h->queue_lock,&irql);
 if(h->closing||!h->consumer_online||h->consumer_stop)status=STATUS_DEVICE_NOT_READY;
 else if(h->inflight>=SD_QUEUE_LIMIT)status=STATUS_DEVICE_BUSY;
 else if(!ExAcquireRundownProtection(&h->requests))status=STATUS_DEVICE_NOT_READY;
 else {InterlockedIncrement(&h->inflight);h->hardware_users++;KeClearEvent(&h->hardware_idle);status=STATUS_SUCCESS;}
 KeReleaseSpinLock(&h->queue_lock,irql);
 if(!NT_SUCCESS(status)){set_result(irp,status,0);IoCompleteRequest(irp,IO_NO_INCREMENT);drop_work(w);return status;}
 InterlockedIncrement(&w->refs); /* inserter; request reference survives to completion */
 w->saved_context=irp->Tail.Overlay.DriverContext[0];irp->Tail.Overlay.DriverContext[0]=w;IoMarkIrpPending(irp);
 status=IoCsqInsertIrpEx(&h->csq,irp,NULL,NULL);
 if(!NT_SUCCESS(status))SdFinish(w,status,0);
 else KeSetEvent(&h->consumer_wake,IO_NO_INCREMENT,FALSE);
 drop_work(w); /* do not touch the IRP after transferring it to CSQ */
 return STATUS_PENDING;
}
