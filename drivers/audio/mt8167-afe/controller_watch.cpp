/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Arrival ordering only; no controller qualification or timer retries. */
#include "controller_watch.h"
#include <wdmguid.h>
#define TAG 'Wc3M'
struct WatchEvent {LIST_ENTRY entry;BOOLEAN arrival;UNICODE_STRING link;WCHAR text[1];};
struct Mt6630ControllerWatch {
 PDEVICE_OBJECT device;Mt6630ControllerChanged changed;PVOID context,registration;
 PIO_WORKITEM work;KSPIN_LOCK gate;LIST_ENTRY pending;KEVENT idle;
 BOOLEAN scheduled;volatile LONG stopping,failure;ULONG queued;
 A04_HFP_CONTROLLER controller;UNICODE_STRING link;
};
static NTSTATUS Complete(PDEVICE_OBJECT,PIRP,PVOID p){KeSetEvent((PKEVENT)p,IO_NO_INCREMENT,FALSE);return STATUS_MORE_PROCESSING_REQUIRED;}
static NTSTATUS Resolve(PUNICODE_STRING link,A04_HFP_CONTROLLER *out)
{
 PFILE_OBJECT file=NULL;PDEVICE_OBJECT device=NULL;NTSTATUS status;
 RtlZeroMemory(out,sizeof(*out));status=IoGetDeviceObjectPointer(link,FILE_READ_DATA,&file,&device);if(!NT_SUCCESS(status))return status;
 a04_mt6630_controller_interface iface={0};KEVENT event;KeInitializeEvent(&event,NotificationEvent,FALSE);
 PIRP irp=IoAllocateIrp(device->StackSize,FALSE);
 if(!irp){ObDereferenceObject(file);return STATUS_INSUFFICIENT_RESOURCES;}
 PIO_STACK_LOCATION stack=IoGetNextIrpStackLocation(irp);stack->MajorFunction=IRP_MJ_PNP;stack->MinorFunction=IRP_MN_QUERY_INTERFACE;
 stack->Parameters.QueryInterface.InterfaceType=&A04Mt6630ControllerInterfaceGuid;stack->Parameters.QueryInterface.Size=sizeof(iface);
 stack->Parameters.QueryInterface.Version=1;stack->Parameters.QueryInterface.Interface=(PINTERFACE)&iface;
 irp->IoStatus.Status=STATUS_NOT_SUPPORTED;IoSetCompletionRoutine(irp,Complete,&event,TRUE,TRUE,TRUE);
 status=IoCallDriver(device,irp);if(status==STATUS_PENDING)KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);
 status=irp->IoStatus.Status;IoFreeIrp(irp);
 if(NT_SUCCESS(status)){
  if(iface.Header.Size!=sizeof(iface)||iface.Header.Version!=1||!iface.Header.Context||!iface.Header.InterfaceDereference||!iface.GetController)status=STATUS_NOT_SUPPORTED;
  else status=iface.GetController(iface.Header.Context,out);
 }
 if(iface.Header.Context&&iface.Header.InterfaceDereference)iface.Header.InterfaceDereference(iface.Header.Context);
 ObDereferenceObject(file);return status;
}
static NTSTATUS Remove(Mt6630ControllerWatch *w)
{
 if(w->controller.context){NTSTATUS s=w->changed(w->context,NULL);if(!NT_SUCCESS(s))return s;
  w->controller.Dereference(w->controller.context);RtlZeroMemory(&w->controller,sizeof(w->controller));}
 if(w->link.Buffer){RtlSecureZeroMemory(w->link.Buffer,w->link.MaximumLength);ExFreePoolWithTag(w->link.Buffer,TAG);RtlZeroMemory(&w->link,sizeof(w->link));}
 return STATUS_SUCCESS;
}
static NTSTATUS Arrive(Mt6630ControllerWatch *w,PUNICODE_STRING link)
{
 if(w->controller.context)return STATUS_SUCCESS;
 A04_HFP_CONTROLLER controller={0};NTSTATUS s=Resolve(link,&controller);
 if(!NT_SUCCESS(s))return s;
 if(controller.size!=sizeof(controller)||controller.version!=A04_SHARED_HFP_VERSION||!controller.context||!controller.Reference||!controller.Dereference||!controller.controller){
  if(controller.context&&controller.Dereference)controller.Dereference(controller.context);return STATUS_NOT_SUPPORTED;}
 PWSTR saved=(PWSTR)ExAllocatePoolWithTag(NonPagedPool,link->Length+sizeof(WCHAR),TAG);
 if(!saved){controller.Dereference(controller.context);return STATUS_INSUFFICIENT_RESOURCES;}
 RtlZeroMemory(saved,link->Length+sizeof(WCHAR));RtlCopyMemory(saved,link->Buffer,link->Length);
 /* Keep the reference even on partial callback failure: its owner may still
  * be registered or hold DMA. Stop must resolve that before dereference. */
 w->controller=controller;w->link.Buffer=saved;w->link.Length=link->Length;w->link.MaximumLength=link->Length+sizeof(WCHAR);
 return w->changed(w->context,&w->controller);
}
static VOID Worker(PDEVICE_OBJECT,PVOID p)
{
 Mt6630ControllerWatch *w=(Mt6630ControllerWatch *)p;
 for(;;){KIRQL irql;KeAcquireSpinLock(&w->gate,&irql);
  if(IsListEmpty(&w->pending)){w->scheduled=FALSE;KeSetEvent(&w->idle,IO_NO_INCREMENT,FALSE);KeReleaseSpinLock(&w->gate,irql);return;}
  WatchEvent *e=CONTAINING_RECORD(RemoveHeadList(&w->pending),WatchEvent,entry);w->queued--;KeReleaseSpinLock(&w->gate,irql);
  if(!w->stopping&&!w->failure){NTSTATUS s=STATUS_SUCCESS;
   if(e->arrival)s=Arrive(w,&e->link);
   else if(w->controller.context&&RtlEqualUnicodeString(&w->link,&e->link,TRUE))s=Remove(w);
   if(!NT_SUCCESS(s)&&s!=STATUS_NO_SUCH_DEVICE&&s!=STATUS_DEVICE_REMOVED&&s!=STATUS_DEVICE_NOT_CONNECTED&&s!=STATUS_NOT_SUPPORTED)InterlockedExchange(&w->failure,(LONG)s);
  }
  RtlSecureZeroMemory(e->text,e->link.MaximumLength);ExFreePoolWithTag(e,TAG);
 }
}
static NTSTATUS Notify(PVOID p,PVOID context)
{
 Mt6630ControllerWatch *w=(Mt6630ControllerWatch *)context;PDEVICE_INTERFACE_CHANGE_NOTIFICATION change=(PDEVICE_INTERFACE_CHANGE_NOTIFICATION)p;
 BOOLEAN arrival=IsEqualGUIDAligned(change->Event,GUID_DEVICE_INTERFACE_ARRIVAL);
 if(!arrival&&!IsEqualGUIDAligned(change->Event,GUID_DEVICE_INTERFACE_REMOVAL))return STATUS_SUCCESS;
 if(w->stopping)return STATUS_SUCCESS;PUNICODE_STRING link=change->SymbolicLinkName;
 if(!link||!link->Buffer||!link->Length||(link->Length&1)||link->Length>4096){InterlockedExchange(&w->failure,STATUS_INVALID_PARAMETER);return STATUS_SUCCESS;}
 SIZE_T bytes=FIELD_OFFSET(WatchEvent,text)+link->Length+sizeof(WCHAR);WatchEvent *e=(WatchEvent *)ExAllocatePoolWithTag(NonPagedPool,bytes,TAG);
 if(!e){InterlockedExchange(&w->failure,STATUS_INSUFFICIENT_RESOURCES);return STATUS_SUCCESS;}RtlZeroMemory(e,bytes);
 e->arrival=arrival;e->link.Buffer=e->text;e->link.Length=link->Length;e->link.MaximumLength=link->Length+sizeof(WCHAR);RtlCopyMemory(e->text,link->Buffer,link->Length);
 KIRQL irql;BOOLEAN queue=FALSE,drop=FALSE;KeAcquireSpinLock(&w->gate,&irql);
 if(w->stopping||w->failure)drop=TRUE;
 else if(w->queued==64){drop=TRUE;InterlockedExchange(&w->failure,STATUS_INSUFFICIENT_RESOURCES);}
 else {InsertTailList(&w->pending,&e->entry);w->queued++;if(!w->scheduled){w->scheduled=TRUE;KeClearEvent(&w->idle);queue=TRUE;}}
 KeReleaseSpinLock(&w->gate,irql);
 if(drop){RtlSecureZeroMemory(e->text,e->link.MaximumLength);ExFreePoolWithTag(e,TAG);}
 else if(queue)IoQueueWorkItem(w->work,Worker,DelayedWorkQueue,w);return STATUS_SUCCESS;
}
NTSTATUS Mt6630ControllerWatchStart(PDEVICE_OBJECT device,Mt6630ControllerChanged changed,PVOID context,Mt6630ControllerWatch **out)
{
 if(!out||!device||!changed||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;*out=NULL;
 Mt6630ControllerWatch *w=(Mt6630ControllerWatch *)ExAllocatePoolWithTag(NonPagedPool,sizeof(*w),TAG);if(!w)return STATUS_INSUFFICIENT_RESOURCES;
 RtlZeroMemory(w,sizeof(*w));w->work=IoAllocateWorkItem(device);if(!w->work){ExFreePoolWithTag(w,TAG);return STATUS_INSUFFICIENT_RESOURCES;}
 w->device=device;ObReferenceObject(device);w->changed=changed;w->context=context;KeInitializeSpinLock(&w->gate);InitializeListHead(&w->pending);KeInitializeEvent(&w->idle,NotificationEvent,TRUE);*out=w;
 return IoRegisterPlugPlayNotification(EventCategoryDeviceInterfaceChange,PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,(PVOID)&A04Mt6630ControllerReadyGuid,device->DriverObject,Notify,w,&w->registration);
}
NTSTATUS Mt6630ControllerWatchStop(Mt6630ControllerWatch *w)
{
 if(!w||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;InterlockedExchange(&w->stopping,1);
 if(w->registration){NTSTATUS s=IoUnregisterPlugPlayNotificationEx(w->registration);if(!NT_SUCCESS(s))return s;w->registration=NULL;}
 LARGE_INTEGER timeout;timeout.QuadPart=-100000000;
 if(KeWaitForSingleObject(&w->idle,Executive,KernelMode,FALSE,&timeout)!=STATUS_SUCCESS)return STATUS_IO_TIMEOUT;
 KIRQL irql;KeAcquireSpinLock(&w->gate,&irql);BOOLEAN busy=w->scheduled||!IsListEmpty(&w->pending);KeReleaseSpinLock(&w->gate,irql);if(busy)return STATUS_DEVICE_BUSY;
 NTSTATUS status=Remove(w);if(!NT_SUCCESS(status))return status;
 IoFreeWorkItem(w->work);ObDereferenceObject(w->device);RtlSecureZeroMemory(w,sizeof(*w));ExFreePoolWithTag(w,TAG);return STATUS_SUCCESS;
}
