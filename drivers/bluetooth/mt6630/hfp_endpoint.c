/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Windows 8.1 bthhfpddi fixed request client + shared speaker/mic MRG lifetime.
 * Protocol facts: Microsoft sysvad/BthhfpDevice.cpp, StreamStart/Suspend,
 * GetBthHfpCodecId and immediate connection/status update requests.
 */
#include "hfp_endpoint.h"
#include <windef.h>
#include <initguid.h>
#include <bthhfpddi.h>
#define TAG 'h36M'
struct hfp_call;
struct a04_bt_hfp_session {
 volatile LONG refs,failure,stopping,uncertain_io,detached,claims[2];KEVENT serial;
 PFILE_OBJECT file;PDEVICE_OBJECT target;PIO_WORKITEM finalize;
 struct a04_bt_hfp_provider provider;struct a04_bt_sideband sideband;
 ULONG running;BOOLEAN link_open;struct hfp_call *watch;
};
struct hfp_call {
 volatile LONG refs;PIRP irp;PFILE_OBJECT file;KEVENT done;UCHAR data[16];
 struct a04_bt_hfp_session *session;BOOLEAN watch;
};
static void lock(struct a04_bt_hfp_session *s){(void)KeWaitForSingleObject(&s->serial,Executive,KernelMode,FALSE,NULL);}
static void unlock(struct a04_bt_hfp_session *s){KeSetEvent(&s->serial,IO_NO_INCREMENT,FALSE);}
static NTSTATUS wait(KEVENT *done){LARGE_INTEGER t;t.QuadPart=-20000000;return KeWaitForSingleObject(done,Executive,KernelMode,FALSE,&t);}
static void failed(struct a04_bt_hfp_session *s,NTSTATUS status)
{InterlockedExchange(&s->failure,(LONG)status);s->provider.fault(s->provider.context,status);}
static void drop_call(struct hfp_call *c)
{
 if(!InterlockedDecrement(&c->refs)){
  struct a04_bt_hfp_session *s=c->session;IoFreeIrp(c->irp);ObDereferenceObject(c->file);
  RtlSecureZeroMemory(c,sizeof(*c));ExFreePoolWithTag(c,TAG);if(s)a04_bt_hfp_dereference(s);
 }
}
static NTSTATUS complete(PDEVICE_OBJECT device,PIRP irp,PVOID context)
{
 struct hfp_call *c=context;UNREFERENCED_PARAMETER(device);
 if(c->watch&&!InterlockedCompareExchange(&c->session->stopping,0,0)){
  NTSTATUS status=irp->IoStatus.Status;
  if(NT_SUCCESS(status)){
   if(irp->IoStatus.Information!=sizeof(NTSTATUS))status=STATUS_DATA_ERROR;
   else RtlCopyMemory(&status,c->data,sizeof(status));
  }
  if(status!=STATUS_SUCCESS)failed(c->session,NT_SUCCESS(status)?STATUS_DEVICE_NOT_READY:status);
 }
 KeSetEvent(&c->done,IO_NO_INCREMENT,FALSE);drop_call(c);return STATUS_MORE_PROCESSING_REQUIRED;
}
static struct hfp_call *make_call(struct a04_bt_hfp_session *s,ULONG code,ULONG in,ULONG out,ULONG value,BOOLEAN watch)
{
 struct hfp_call *c;PIO_STACK_LOCATION sp;
 c=ExAllocatePoolWithTag(NonPagedPool,sizeof(*c),TAG);if(!c)return NULL;RtlZeroMemory(c,sizeof(*c));
 c->irp=IoAllocateIrp(s->target->StackSize,FALSE);if(!c->irp){ExFreePoolWithTag(c,TAG);return NULL;}
 c->refs=2;c->file=s->file;ObReferenceObject(c->file);c->watch=watch;
 if(watch){c->session=s;a04_bt_hfp_reference(s);}
 KeInitializeEvent(&c->done,NotificationEvent,FALSE);RtlCopyMemory(c->data,&value,sizeof(value));
 c->irp->RequestorMode=KernelMode;c->irp->UserBuffer=out?c->data:NULL;c->irp->IoStatus.Status=STATUS_NOT_SUPPORTED;
 sp=IoGetNextIrpStackLocation(c->irp);sp->MajorFunction=IRP_MJ_DEVICE_CONTROL;sp->FileObject=c->file;
 sp->Parameters.DeviceIoControl.IoControlCode=code;sp->Parameters.DeviceIoControl.InputBufferLength=in;
 sp->Parameters.DeviceIoControl.OutputBufferLength=out;sp->Parameters.DeviceIoControl.Type3InputBuffer=in?c->data:NULL;
 IoSetCompletionRoutine(c->irp,complete,c,TRUE,TRUE,TRUE);return c;
}
/* All METHOD_NEITHER storage is nonpaged/IRP-owned, never the caller stack.
 * On timeout cancel and return; completion keeps its own call/file reference.
 * No retry and no destruction of late output memory. A timed-out command may
 * have changed link state: its session must remain quarantined, not reopened. */
static NTSTATUS request(struct a04_bt_hfp_session *s,ULONG code,ULONG in,ULONG out,ULONG value,PVOID result)
{
 struct hfp_call *c=make_call(s,code,in,out,value,FALSE);NTSTATUS status;
 if(!c)return STATUS_INSUFFICIENT_RESOURCES;
 (void)IoCallDriver(s->target,c->irp);
 if(wait(&c->done)!=STATUS_SUCCESS){IoCancelIrp(c->irp);status=STATUS_IO_TIMEOUT;InterlockedExchange(&s->uncertain_io,1);failed(s,status);}
 else {
  status=c->irp->IoStatus.Status;
  if(NT_SUCCESS(status)&&c->irp->IoStatus.Information!=out)status=STATUS_DATA_ERROR;
  if(NT_SUCCESS(status)&&out)RtlCopyMemory(result,c->data,out);
 }
 drop_call(c);return status;
}
static NTSTATUS start_watch(struct a04_bt_hfp_session *s)
{
 if(s->watch)return STATUS_INVALID_DEVICE_STATE;
 InterlockedExchange(&s->stopping,0);
 s->watch=make_call(s,IOCTL_BTHHFP_STREAM_GET_STATUS_UPDATE,sizeof(BOOL),sizeof(NTSTATUS),FALSE,TRUE);
 if(!s->watch)return STATUS_INSUFFICIENT_RESOURCES;
 (void)IoCallDriver(s->target,s->watch->irp);return a04_bt_hfp_status(s);
}
static NTSTATUS stop_watch(struct a04_bt_hfp_session *s)
{
 struct hfp_call *c=s->watch;if(!c)return STATUS_SUCCESS;
 InterlockedExchange(&s->stopping,1);IoCancelIrp(c->irp);
 if(wait(&c->done)!=STATUS_SUCCESS){failed(s,STATUS_IO_TIMEOUT);return STATUS_IO_TIMEOUT;}
 s->watch=NULL;drop_call(c);return STATUS_SUCCESS;
}
static NTSTATUS stop_link(struct a04_bt_hfp_session *s)
{
 NTSTATUS status;
 if(InterlockedCompareExchange(&s->uncertain_io,0,0))return STATUS_IO_TIMEOUT;
 if(s->sideband.state==A04_BT_SB_PREPARED||s->sideband.state==A04_BT_SB_RUNNING){
  if(a04_bt_sideband_close(&s->sideband)){failed(s,STATUS_IO_DEVICE_ERROR);return STATUS_IO_DEVICE_ERROR;}
 }
 if(s->sideband.state==A04_BT_SB_FAULT)return STATUS_IO_DEVICE_ERROR;
 status=stop_watch(s);if(!NT_SUCCESS(status))return status;
 if(s->link_open){
  status=request(s,IOCTL_BTHHFP_STREAM_CLOSE,0,0,0,NULL);
  if(!NT_SUCCESS(status)){failed(s,status);return status;}s->link_open=FALSE;
 }
 return STATUS_SUCCESS;
}
static void finalize(PDEVICE_OBJECT device,PVOID context)
{
 struct a04_bt_hfp_session *s=context;UNREFERENCED_PARAMETER(device);
 if(!s->detached||s->running||s->link_open||s->watch||s->uncertain_io||s->sideband.state==A04_BT_SB_FAULT){
  InterlockedExchange(&s->refs,1);failed(s,STATUS_INVALID_DEVICE_STATE);return;
 }
 s->provider.dereference(s->provider.context);ObDereferenceObject(s->file);IoFreeWorkItem(s->finalize);
 RtlSecureZeroMemory(s,sizeof(*s));ExFreePoolWithTag(s,TAG);
}
void a04_bt_hfp_reference(struct a04_bt_hfp_session *s){InterlockedIncrement(&s->refs);}
void a04_bt_hfp_dereference(struct a04_bt_hfp_session *s)
{if(!InterlockedDecrement(&s->refs))IoQueueWorkItem(s->finalize,finalize,DelayedWorkQueue,s);}
NTSTATUS a04_bt_hfp_status(struct a04_bt_hfp_session *s)
{return !s?STATUS_INVALID_PARAMETER:InterlockedCompareExchange(&s->detached,0,0)?STATUS_DEVICE_REMOVED:(NTSTATUS)InterlockedCompareExchange(&s->failure,0,0);}
const struct a04_bt_hfp_provider *a04_bt_hfp_provider(struct a04_bt_hfp_session *s){return s?&s->provider:NULL;}
NTSTATUS a04_bt_hfp_pin_claim(struct a04_bt_hfp_session *s,ULONG d)
{
 if(!s||d>1||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lock(s);NTSTATUS status=s->detached?STATUS_DEVICE_REMOVED:a04_bt_hfp_status(s);
 if(NT_SUCCESS(status)&&InterlockedCompareExchange(&s->claims[d],1,0))status=STATUS_DEVICE_BUSY;
 unlock(s);return status;
}
void a04_bt_hfp_pin_release(struct a04_bt_hfp_session *s,ULONG d){if(s&&d<2)InterlockedExchange(&s->claims[d],0);}
NTSTATUS a04_bt_hfp_run(struct a04_bt_hfp_session *s,ULONG d)
{
 NTSTATUS status;BOOL connected=TRUE;UCHAR codec=0xff;
 if(!s||d>1||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lock(s);status=a04_bt_hfp_status(s);
 if(!NT_SUCCESS(status)||s->detached||!s->claims[d]){if(NT_SUCCESS(status))status=STATUS_DEVICE_NOT_READY;goto done;}
 if(s->running&(1u<<d)){status=STATUS_INVALID_DEVICE_STATE;goto done;}
 if(s->running){
  status=s->provider.set_pin_active(s->provider.context,d,TRUE);
  if(NT_SUCCESS(status))s->running|=1u<<d;else failed(s,status);goto done;
 }
 status=request(s,IOCTL_BTHHFP_DEVICE_GET_CONNECTION_STATUS_UPDATE,sizeof(BOOL),sizeof(BOOL),TRUE,&connected);
 if(!NT_SUCCESS(status))goto done;if(!connected){status=STATUS_DEVICE_NOT_CONNECTED;goto done;}
 status=request(s,IOCTL_BTHHFP_STREAM_OPEN,0,0,0,NULL);
 if(!NT_SUCCESS(status))goto done;s->link_open=TRUE;
 /* WinBlue codec 0 is CVSD (Sysvad); this is not the wire HFP codec enum. */
 status=request(s,IOCTL_BTHHFP_DEVICE_GET_CODEC_ID,sizeof(HFP_BYPASS_CODEC_ID_VERSION),sizeof(HFP_BYPASS_CODEC_ID_V1),REQ_HFP_BYPASS_CODEC_ID_V1,&codec);
 if(!NT_SUCCESS(status))goto cleanup;if(codec!=0){status=STATUS_NOT_SUPPORTED;goto cleanup;}
 status=s->provider.validate_pcm(s->provider.context,codec,8000,1,16);if(!NT_SUCCESS(status))goto cleanup;
 status=start_watch(s);if(!NT_SUCCESS(status))goto cleanup;
 if(s->sideband.state==A04_BT_SB_CLOSED)RtlZeroMemory(&s->sideband,sizeof(s->sideband));
 if(a04_bt_sideband_prepare(&s->sideband,&s->provider.sideband)||a04_bt_sideband_start(&s->sideband)){
  status=STATUS_IO_DEVICE_ERROR;failed(s,status);goto cleanup;
 }
 status=a04_bt_hfp_status(s);if(!NT_SUCCESS(status))goto cleanup;
 status=s->provider.set_pin_active(s->provider.context,d,TRUE);
 if(!NT_SUCCESS(status)){failed(s,status);goto cleanup;}
 s->running=1u<<d;goto done;
cleanup:
 if(!NT_SUCCESS(stop_link(s)))status=STATUS_IO_DEVICE_ERROR;
done:unlock(s);return status;
}
NTSTATUS a04_bt_hfp_suspend(struct a04_bt_hfp_session *s,ULONG d)
{
 NTSTATUS status=STATUS_SUCCESS;
 if(!s||d>1||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lock(s);
 if(s->running&(1u<<d)){
  status=s->provider.set_pin_active(s->provider.context,d,FALSE);
  if(!NT_SUCCESS(status)){failed(s,status);unlock(s);return status;}
  s->running&=~(1u<<d);
 }
 if(!s->running)status=stop_link(s);unlock(s);return status;
}
NTSTATUS a04_bt_hfp_detach(struct a04_bt_hfp_session *s)
{
 NTSTATUS status;if(!s||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lock(s);InterlockedExchange(&s->detached,1);s->running=0;status=stop_link(s);
 unlock(s);return status;
}
NTSTATUS a04_bt_hfp_attach(PDEVICE_OBJECT owner,PCUNICODE_STRING link,const struct a04_bt_hfp_provider *p,struct a04_bt_hfp_session **out)
{
 struct a04_bt_hfp_session *s;PWSTR links=NULL,at;BOOLEAN found=FALSE;NTSTATUS status;
 if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
 if(!owner||!link||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL||p->size!=sizeof(*p)||p->version!=A04_BT_HFP_PROVIDER_VERSION||
    !p->reference||!p->dereference||!p->validate_pcm||!p->allocate||!p->buffer_live||!p->free_drained||!p->set_pin_active||!p->position||!p->device_description||!p->latency||!p->fault||!p->retire||
    !p->sideband.acquire||!p->sideband.live||!p->sideband.read_afe||!p->sideband.update_afe||!p->sideband.pins||
    !p->sideband.prepare_dma||!p->sideband.start_dma||!p->sideband.stop_dma_drained||!p->sideband.delay_us||!p->sideband.release_quiescent)return STATUS_INVALID_PARAMETER;
 status=IoGetDeviceInterfaces(&GUID_DEVINTERFACE_BLUETOOTH_HFP_SCO_HCIBYPASS,NULL,0,&links);if(!NT_SUCCESS(status))return status;
 for(at=links;*at;at+=wcslen(at)+1){UNICODE_STRING name;RtlInitUnicodeString(&name,at);if(RtlEqualUnicodeString(&name,link,FALSE)){found=TRUE;break;}}
 ExFreePool(links);if(!found)return STATUS_NO_SUCH_DEVICE;
 s=ExAllocatePoolWithTag(NonPagedPool,sizeof(*s),TAG);if(!s)return STATUS_INSUFFICIENT_RESOURCES;RtlZeroMemory(s,sizeof(*s));
 status=IoGetDeviceObjectPointer((PUNICODE_STRING)link,FILE_READ_DATA|FILE_WRITE_DATA,&s->file,&s->target);
 if(!NT_SUCCESS(status)){ExFreePoolWithTag(s,TAG);return status;}
 s->finalize=IoAllocateWorkItem(owner);if(!s->finalize){ObDereferenceObject(s->file);ExFreePoolWithTag(s,TAG);return STATUS_INSUFFICIENT_RESOURCES;}
 s->provider=*p;s->provider.reference(s->provider.context);s->refs=1;KeInitializeEvent(&s->serial,SynchronizationEvent,TRUE);*out=s;return STATUS_SUCCESS;
}
NTSTATUS a04_bt_hfp_register_notifications(PDRIVER_OBJECT driver,PDRIVER_NOTIFICATION_CALLBACK_ROUTINE callback,PVOID context,PVOID *registration)
{
 if(!driver||!callback||!registration||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 return IoRegisterPlugPlayNotification(EventCategoryDeviceInterfaceChange,PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
  (PVOID)&GUID_DEVINTERFACE_BLUETOOTH_HFP_SCO_HCIBYPASS,driver,callback,context,registration);
}
NTSTATUS a04_bt_hfp_unregister_notifications(PVOID registration)
{return KeGetCurrentIrql()==PASSIVE_LEVEL&&registration?IoUnregisterPlugPlayNotificationEx(registration):STATUS_INVALID_PARAMETER;}
