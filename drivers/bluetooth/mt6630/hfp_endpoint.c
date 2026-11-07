/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Windows 8.1 bthhfpddi fixed request client + shared speaker/mic MRG lifetime.
 * Protocol facts: Microsoft sysvad/BthhfpDevice.cpp, StreamStart/Suspend,
 * GetBthHfpCodecId and immediate connection/status update requests.
 */
#include "hfp_endpoint.h"
#include <windef.h>
#include <initguid.h>
#include <bthhfpddi.h>
#include <ks.h>
#define TAG 'h36M'
struct hfp_call;struct hfp_events;struct hfp_notice;
static void notice_complete(struct hfp_notice *);
struct a04_bt_hfp_session {
 volatile LONG refs,failure,stopping,uncertain_io,detached,claims[2];KEVENT serial;
 PFILE_OBJECT file;PDEVICE_OBJECT target,owner;PIO_WORKITEM finalize;
 struct a04_bt_hfp_provider provider;struct a04_bt_sideband sideband;
 ULONG running;BOOLEAN link_open;struct hfp_call *watch;struct hfp_events *events;
};
struct hfp_call {
 volatile LONG refs;PIRP irp;PFILE_OBJECT file;KEVENT done;UCHAR data[4096];
 struct a04_bt_hfp_session *session;BOOLEAN watch;struct hfp_notice *notice;
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
 if(c->notice)notice_complete(c->notice);KeSetEvent(&c->done,IO_NO_INCREMENT,FALSE);drop_call(c);return STATUS_MORE_PROCESSING_REQUIRED;
}
static struct hfp_call *make_call(struct a04_bt_hfp_session *s,ULONG code,ULONG in,ULONG out,ULONG value,BOOLEAN watch)
{
 struct hfp_call *c;PIO_STACK_LOCATION sp;
 if(in>4096||out>4096)return NULL;
 c=ExAllocatePoolWithTag(NonPagedPool,sizeof(*c),TAG);if(!c)return NULL;RtlZeroMemory(c,sizeof(*c));
 c->irp=IoAllocateIrp(s->target->StackSize,FALSE);if(!c->irp){ExFreePoolWithTag(c,TAG);return NULL;}
 c->refs=2;c->file=s->file;ObReferenceObject(c->file);c->watch=watch;
 if(watch){c->session=s;a04_bt_hfp_reference(s);}
 KeInitializeEvent(&c->done,NotificationEvent,FALSE);RtlCopyMemory(c->data,&value,sizeof(value));
 c->irp->RequestorMode=KernelMode;c->irp->UserBuffer=out?c->data:NULL;c->irp->IoStatus.Status=STATUS_NOT_SUPPORTED;c->irp->IoStatus.Information=0;
 sp=IoGetNextIrpStackLocation(c->irp);sp->MajorFunction=IRP_MJ_DEVICE_CONTROL;sp->FileObject=c->file;
 sp->Parameters.DeviceIoControl.IoControlCode=code;sp->Parameters.DeviceIoControl.InputBufferLength=in;
 sp->Parameters.DeviceIoControl.OutputBufferLength=out;sp->Parameters.DeviceIoControl.Type3InputBuffer=in?c->data:NULL;
 IoSetCompletionRoutine(c->irp,complete,c,TRUE,TRUE,TRUE);return c;
}
/* All METHOD_NEITHER storage is nonpaged/IRP-owned, never the caller stack.
 * On timeout cancel and return; completion keeps its own call/file reference.
 * No retry and no destruction of late output memory. A timed-out command may
 * have changed link state: its session must remain quarantined, not reopened. */
static NTSTATUS request_sized(struct a04_bt_hfp_session *s,ULONG code,ULONG in,ULONG out,ULONG value,PVOID result,ULONG_PTR *actual)
{
 struct hfp_call *c=make_call(s,code,in,out,value,FALSE);NTSTATUS status;
 if(!c)return STATUS_INSUFFICIENT_RESOURCES;
 (void)IoCallDriver(s->target,c->irp);
 if(wait(&c->done)!=STATUS_SUCCESS){IoCancelIrp(c->irp);status=STATUS_IO_TIMEOUT;InterlockedExchange(&s->uncertain_io,1);failed(s,status);}
 else {
  status=c->irp->IoStatus.Status;
  if(actual)*actual=c->irp->IoStatus.Information;
  if(NT_SUCCESS(status)&&c->irp->IoStatus.Information>out)status=STATUS_DATA_ERROR;
  if(NT_SUCCESS(status)&&out)RtlCopyMemory(result,c->data,c->irp->IoStatus.Information);
 }
 drop_call(c);return status;
}
static NTSTATUS request(struct a04_bt_hfp_session *s,ULONG code,ULONG in,ULONG out,ULONG value,PVOID result)
{
 ULONG_PTR actual=0;NTSTATUS status=request_sized(s,code,in,out,value,result,&actual);
 return NT_SUCCESS(status)&&actual!=out?STATUS_DATA_ERROR:status;
}
NTSTATUS a04_bt_hfp_metadata(struct a04_bt_hfp_session *s,struct a04_bt_hfp_metadata *out)
{
 ULONG_PTR needed=0,actual=0;BTHHFP_DESCRIPTOR2 *d=NULL;NTSTATUS status;
 if(!s||!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(out,sizeof(*out));lock(s);status=a04_bt_hfp_status(s);if(!NT_SUCCESS(status))goto done;
 status=request_sized(s,IOCTL_BTHHFP_DEVICE_GET_DESCRIPTOR2,0,0,0,NULL,&needed);
 if(status!=STATUS_BUFFER_TOO_SMALL){if(NT_SUCCESS(status))status=STATUS_DATA_ERROR;goto done;}
 if(needed<sizeof(*d)||needed>4096){status=STATUS_INVALID_BUFFER_SIZE;goto done;}
 d=ExAllocatePoolWithTag(NonPagedPool,(SIZE_T)needed,TAG);if(!d){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 RtlZeroMemory(d,(SIZE_T)needed);
 status=request_sized(s,IOCTL_BTHHFP_DEVICE_GET_DESCRIPTOR2,0,(ULONG)needed,0,d,&actual);
 if(NT_SUCCESS(status)){
  if(actual<sizeof(*d))status=STATUS_DATA_ERROR;
  else {out->input_category=d->InputPinCategory;out->output_category=d->OutputPinCategory;
   out->container=d->ContainerId;out->volume=!!d->SupportsVolume;out->nrec=!!d->SupportsNREC;out->volume_values_size=d->VolumePropertyValuesSize;}
 }
 /* FriendlyName is intentionally neither dereferenced, retained nor logged. */
 if(d){RtlSecureZeroMemory(d,(SIZE_T)needed);ExFreePoolWithTag(d,TAG);}
done:unlock(s);return status;
}
NTSTATUS a04_bt_hfp_connection(struct a04_bt_hfp_session *s,BOOLEAN *connected)
{
 NTSTATUS status;BOOL value=FALSE;
 if(!s||!connected||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 *connected=FALSE;lock(s);status=a04_bt_hfp_status(s);
 if(NT_SUCCESS(status))status=request(s,IOCTL_BTHHFP_DEVICE_GET_CONNECTION_STATUS_UPDATE,sizeof(BOOL),sizeof(BOOL),TRUE,&value);
 if(NT_SUCCESS(status))*connected=!!value;unlock(s);return status;
}
static const GUID volume_type_general={STATIC_KSPROPTYPESETID_General};
/* Bounds and steps are returned by HFP, never a guessed 0..15/dB mapping. */
static int in_output(struct hfp_call *c,ULONG_PTR bytes,const void *p,SIZE_T length)
{
 ULONG_PTR base=(ULONG_PTR)c->data,at=(ULONG_PTR)p;
 return !(at&3)&&at>=base&&length<=bytes&&at-base<=bytes-length;
}
NTSTATUS a04_bt_hfp_volume_range(struct a04_bt_hfp_session *s,ULONG bytes,struct a04_bt_hfp_volume_range *out)
{
 struct hfp_call *c;NTSTATUS status;
 if(!s||!out||KeGetCurrentIrql()!=PASSIVE_LEVEL||bytes<sizeof(KSPROPERTY_VALUES)||bytes>4096)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(out,sizeof(*out));lock(s);status=a04_bt_hfp_status(s);if(!NT_SUCCESS(status))goto done;
 c=make_call(s,IOCTL_BTHHFP_DEVICE_GET_VOLUMEPROPERTYVALUES,0,bytes,0,FALSE);
 if(!c){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}(void)IoCallDriver(s->target,c->irp);
 if(wait(&c->done)!=STATUS_SUCCESS){IoCancelIrp(c->irp);InterlockedExchange(&s->uncertain_io,1);status=STATUS_IO_TIMEOUT;failed(s,status);}
 else {
  ULONG_PTR actual=c->irp->IoStatus.Information;status=c->irp->IoStatus.Status;
  if(NT_SUCCESS(status)){
   KSPROPERTY_VALUES *v=(KSPROPERTY_VALUES *)c->data;
   status=STATUS_DATA_ERROR;
   if(actual>=sizeof(*v)&&actual<=bytes&&v->MembersListCount==1&&
      IsEqualGUID(&v->PropTypeSet.Set,&volume_type_general)&&v->PropTypeSet.Id==3&&
      in_output(c,actual,v->MembersList,sizeof(KSPROPERTY_MEMBERSLIST))){
    const KSPROPERTY_MEMBERSLIST *list=v->MembersList;
    if(list->MembersHeader.MembersFlags==KSPROPERTY_MEMBER_STEPPEDRANGES&&
       list->MembersHeader.MembersSize==sizeof(KSPROPERTY_STEPPING_LONG)&&list->MembersHeader.MembersCount==1&&
       in_output(c,actual,list->Members,sizeof(KSPROPERTY_STEPPING_LONG))){
     const KSPROPERTY_STEPPING_LONG *r=list->Members;
     if(r->SteppingDelta&&r->Bounds.SignedMinimum<=r->Bounds.SignedMaximum){
      out->minimum=r->Bounds.SignedMinimum;out->maximum=r->Bounds.SignedMaximum;out->step=r->SteppingDelta;out->member_flags=list->MembersHeader.Flags;status=STATUS_SUCCESS;
     }
    }else status=STATUS_NOT_SUPPORTED;
   }
  }
 }
 drop_call(c);
done:unlock(s);return status;
}
static ULONG volume_code(ULONG d,BOOLEAN set)
{return set?(d?IOCTL_BTHHFP_MIC_SET_VOLUME:IOCTL_BTHHFP_SPEAKER_SET_VOLUME):(d?IOCTL_BTHHFP_MIC_GET_VOLUME_STATUS_UPDATE:IOCTL_BTHHFP_SPEAKER_GET_VOLUME_STATUS_UPDATE);}
NTSTATUS a04_bt_hfp_volume_get(struct a04_bt_hfp_session *s,ULONG d,LONG *out)
{
 NTSTATUS status;if(!s||d>1||!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lock(s);status=a04_bt_hfp_status(s);if(NT_SUCCESS(status))status=request(s,volume_code(d,FALSE),sizeof(BOOL),sizeof(LONG),TRUE,out);unlock(s);return status;
}
NTSTATUS a04_bt_hfp_volume_set(struct a04_bt_hfp_session *s,ULONG d,LONG value)
{
 NTSTATUS status;if(!s||d>1||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lock(s);status=a04_bt_hfp_status(s);if(NT_SUCCESS(status))status=request(s,volume_code(d,TRUE),sizeof(LONG),0,(ULONG)value,NULL);unlock(s);return status;
}
struct a04_bt_hfp_observer {EX_RUNDOWN_REF rundown;PVOID context;void (*changed)(PVOID,ULONG);ULONG direction;};
struct hfp_notice {struct hfp_events *events;struct hfp_call *call;ULONG index;LONG last;};
struct hfp_events {
 struct a04_bt_hfp_session *session;PIO_WORKITEM work;KEVENT serial,idle;KSPIN_LOCK gate;
 volatile LONG stopping;ULONG pending;BOOLEAN scheduled,volume;
 struct hfp_notice notices[3];struct a04_bt_hfp_observer *observers[2];
};
static void events_worker(PDEVICE_OBJECT,PVOID);
static void events_lock(struct hfp_events *e){(void)KeWaitForSingleObject(&e->serial,Executive,KernelMode,FALSE,NULL);}
static void events_unlock(struct hfp_events *e){KeSetEvent(&e->serial,IO_NO_INCREMENT,FALSE);}
static void notice_complete(struct hfp_notice *n)
{
 struct hfp_events *e=n->events;KIRQL irql;BOOLEAN queue=FALSE;KeAcquireSpinLock(&e->gate,&irql);
 if(!e->stopping){e->pending|=1u<<n->index;if(!e->scheduled){e->scheduled=TRUE;KeClearEvent(&e->idle);queue=TRUE;}}
 KeReleaseSpinLock(&e->gate,irql);if(queue)IoQueueWorkItem(e->work,events_worker,DelayedWorkQueue,e);
}
static void emit_change(struct hfp_events *e,ULONG index)
{
 for(ULONG d=0;d<2;d++)if(!index||d==index-1){
  KIRQL irql;struct a04_bt_hfp_observer *o;KeAcquireSpinLock(&e->gate,&irql);o=e->observers[d];
  if(o&&!ExAcquireRundownProtection(&o->rundown))o=NULL;KeReleaseSpinLock(&e->gate,irql);
  if(o){o->changed(o->context,index?A04_BT_HFP_CHANGE_VOLUME:A04_BT_HFP_CHANGE_CONNECTION);ExReleaseRundownProtection(&o->rundown);}
 }
}
static NTSTATUS arm_notice(struct hfp_notice *n)
{
 ULONG code=n->index?volume_code(n->index-1,FALSE):IOCTL_BTHHFP_DEVICE_GET_CONNECTION_STATUS_UPDATE;
 n->call=make_call(n->events->session,code,sizeof(BOOL),sizeof(LONG),FALSE,FALSE);
 if(!n->call)return STATUS_INSUFFICIENT_RESOURCES;n->call->notice=n;
 (void)IoCallDriver(n->events->session->target,n->call->irp);return STATUS_SUCCESS;
}
static void events_worker(PDEVICE_OBJECT device,PVOID context)
{
 struct hfp_events *e=context;ULONG budget=0;UNREFERENCED_PARAMETER(device);events_lock(e);
 for(;;){
  ULONG index=3;KIRQL irql;KeAcquireSpinLock(&e->gate,&irql);
  if(!e->stopping)for(ULONG i=0;i<3;i++)if(e->pending&(1u<<i)){index=i;e->pending&=~(1u<<i);break;}
  if(index==3){e->pending=0;e->scheduled=FALSE;KeSetEvent(&e->idle,IO_NO_INCREMENT,FALSE);KeReleaseSpinLock(&e->gate,irql);events_unlock(e);return;}
  KeReleaseSpinLock(&e->gate,irql);
  struct hfp_notice *n=&e->notices[index];struct hfp_call *c=n->call;NTSTATUS status=c->irp->IoStatus.Status;LONG value=0;
  if(NT_SUCCESS(status)&&c->irp->IoStatus.Information!=sizeof(value))status=STATUS_DATA_ERROR;
  if(NT_SUCCESS(status))RtlCopyMemory(&value,c->data,sizeof(value));
  /* A FALSE/immediate notification completing repeatedly with no state change
   * is not progress. Never spin/retry a failing or broken HFP target. */
  if(NT_SUCCESS(status)&&++budget>64)status=STATUS_DATA_ERROR;
  n->call=NULL;drop_call(c);
  if(!NT_SUCCESS(status)){InterlockedExchange(&e->stopping,1);failed(e->session,status);continue;}
  if(!index)value=!!value;
  if(value!=n->last){n->last=value;emit_change(e,index);}
  status=arm_notice(n);
  if(!NT_SUCCESS(status)){InterlockedExchange(&e->stopping,1);failed(e->session,status);}
 }
}
NTSTATUS a04_bt_hfp_events_start(struct a04_bt_hfp_session *s,BOOLEAN volume)
{
 struct hfp_events *e;NTSTATUS status;
 if(!s||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lock(s);status=a04_bt_hfp_status(s);if(!NT_SUCCESS(status))goto done;
 if(s->events){status=STATUS_INVALID_DEVICE_STATE;goto done;}
 e=ExAllocatePoolWithTag(NonPagedPool,sizeof(*e),TAG);if(!e){status=STATUS_INSUFFICIENT_RESOURCES;goto done;}RtlZeroMemory(e,sizeof(*e));
 e->work=IoAllocateWorkItem(s->owner);if(!e->work){ExFreePoolWithTag(e,TAG);status=STATUS_INSUFFICIENT_RESOURCES;goto done;}
 e->session=s;e->volume=volume;KeInitializeEvent(&e->serial,SynchronizationEvent,TRUE);KeInitializeEvent(&e->idle,NotificationEvent,TRUE);KeInitializeSpinLock(&e->gate);
 a04_bt_hfp_reference(s);s->events=e;events_lock(e);
 for(ULONG i=0;i<(volume?3u:1u);i++){
  struct hfp_notice *n=&e->notices[i];n->events=e;n->index=i;
  status=request(s,i?volume_code(i-1,FALSE):IOCTL_BTHHFP_DEVICE_GET_CONNECTION_STATUS_UPDATE,sizeof(BOOL),sizeof(LONG),TRUE,&n->last);
  if(!NT_SUCCESS(status))break;if(!i)n->last=!!n->last;
 }
 if(NT_SUCCESS(status))for(ULONG i=0;i<(volume?3u:1u);i++){status=arm_notice(&e->notices[i]);if(!NT_SUCCESS(status))break;}
 if(!NT_SUCCESS(status))InterlockedExchange(&e->stopping,1);events_unlock(e);
done:unlock(s);return status;
}
NTSTATUS a04_bt_hfp_observe(struct a04_bt_hfp_session *s,ULONG d,void (*changed)(PVOID,ULONG),PVOID context,struct a04_bt_hfp_observer **out)
{
 NTSTATUS status;struct a04_bt_hfp_observer *o;
 if(!s||d>1||!changed||!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;*out=NULL;
 o=ExAllocatePoolWithTag(NonPagedPool,sizeof(*o),TAG);if(!o)return STATUS_INSUFFICIENT_RESOURCES;
 ExInitializeRundownProtection(&o->rundown);o->context=context;o->changed=changed;o->direction=d;
 lock(s);struct hfp_events *e=s->events;status=a04_bt_hfp_status(s);
 if(NT_SUCCESS(status)&&(!e||e->stopping))status=STATUS_DEVICE_NOT_READY;
 if(NT_SUCCESS(status)){
  KIRQL irql;KeAcquireSpinLock(&e->gate,&irql);
  if(e->observers[d])status=STATUS_DEVICE_BUSY;else e->observers[d]=o;KeReleaseSpinLock(&e->gate,irql);
 }
 unlock(s);if(!NT_SUCCESS(status))ExFreePoolWithTag(o,TAG);else *out=o;return status;
}
NTSTATUS a04_bt_hfp_unobserve(struct a04_bt_hfp_session *s,struct a04_bt_hfp_observer *o)
{
 if(!s||!o||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lock(s);struct hfp_events *e=s->events;NTSTATUS status=STATUS_INVALID_DEVICE_STATE;
 if(e){KIRQL irql;KeAcquireSpinLock(&e->gate,&irql);if(e->observers[o->direction]==o){e->observers[o->direction]=NULL;status=STATUS_SUCCESS;}KeReleaseSpinLock(&e->gate,irql);}
 if(NT_SUCCESS(status)){ExWaitForRundownProtectionRelease(&o->rundown);ExFreePoolWithTag(o,TAG);}unlock(s);return status;
}
NTSTATUS a04_bt_hfp_events_stop(struct a04_bt_hfp_session *s)
{
 NTSTATUS status=STATUS_SUCCESS;struct hfp_events *e;
 if(!s||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 lock(s);e=s->events;if(!e)goto done;
 events_lock(e);KIRQL irql;KeAcquireSpinLock(&e->gate,&irql);BOOLEAN observed=e->observers[0]||e->observers[1];
 if(!observed)e->stopping=1;KeReleaseSpinLock(&e->gate,irql);
 if(observed){events_unlock(e);status=STATUS_DEVICE_BUSY;goto done;}
 for(ULONG i=0;i<3;i++)if(e->notices[i].call){struct hfp_call *c=e->notices[i].call;IoCancelIrp(c->irp);if(wait(&c->done)!=STATUS_SUCCESS)status=STATUS_IO_TIMEOUT;}
 events_unlock(e);
 if(!NT_SUCCESS(status)){failed(s,status);goto done;}
 if(wait(&e->idle)!=STATUS_SUCCESS){status=STATUS_IO_TIMEOUT;failed(s,status);goto done;}
 /* Idle is signalled just before the worker releases serial. Join that gate
  * as well: neither engine nor its work item may be freed before its last use. */
 events_lock(e);events_unlock(e);
 for(ULONG i=0;i<3;i++)if(e->notices[i].call)drop_call(e->notices[i].call);
 IoFreeWorkItem(e->work);s->events=NULL;RtlSecureZeroMemory(e,sizeof(*e));ExFreePoolWithTag(e,TAG);a04_bt_hfp_dereference(s);
done:unlock(s);return status;
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
 if(!s->detached||s->running||s->link_open||s->watch||s->events||s->uncertain_io||s->sideband.state==A04_BT_SB_FAULT){
  InterlockedExchange(&s->refs,1);failed(s,STATUS_INVALID_DEVICE_STATE);return;
 }
 s->provider.dereference(s->provider.context);ObDereferenceObject(s->file);IoFreeWorkItem(s->finalize);ObDereferenceObject(s->owner);
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
 lock(s);InterlockedExchange(&s->detached,1);unlock(s);
 status=a04_bt_hfp_events_stop(s);if(!NT_SUCCESS(status))return status;
 lock(s);s->running=0;status=stop_link(s);
 unlock(s);return status;
}
NTSTATUS a04_bt_hfp_attach(PDEVICE_OBJECT owner,PCUNICODE_STRING link,const struct a04_bt_hfp_provider *p,struct a04_bt_hfp_session **out)
{
 struct a04_bt_hfp_session *s;PWSTR links=NULL,at;BOOLEAN found=FALSE;NTSTATUS status;
 if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
 if(!owner||!link||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL||p->size!=sizeof(*p)||p->version!=A04_BT_HFP_PROVIDER_VERSION||
    !p->reference||!p->dereference||!p->match_hfp_target||!p->validate_pcm||!p->allocate||!p->buffer_live||!p->free_drained||!p->set_pin_active||!p->position||!p->device_description||!p->latency||!p->fault||!p->retire||
    !p->sideband.acquire||!p->sideband.live||!p->sideband.read_afe||!p->sideband.update_afe||!p->sideband.pins||
    !p->sideband.prepare_dma||!p->sideband.start_dma||!p->sideband.stop_dma_drained||!p->sideband.delay_us||!p->sideband.release_quiescent)return STATUS_INVALID_PARAMETER;
 status=IoGetDeviceInterfaces(&GUID_DEVINTERFACE_BLUETOOTH_HFP_SCO_HCIBYPASS,NULL,0,&links);if(!NT_SUCCESS(status))return status;
 for(at=links;*at;at+=wcslen(at)+1){UNICODE_STRING name;RtlInitUnicodeString(&name,at);if(RtlEqualUnicodeString(&name,link,FALSE)){found=TRUE;break;}}
 ExFreePool(links);if(!found)return STATUS_NO_SUCH_DEVICE;
 s=ExAllocatePoolWithTag(NonPagedPool,sizeof(*s),TAG);if(!s)return STATUS_INSUFFICIENT_RESOURCES;RtlZeroMemory(s,sizeof(*s));
 status=IoGetDeviceObjectPointer((PUNICODE_STRING)link,FILE_READ_DATA|FILE_WRITE_DATA,&s->file,&s->target);
 if(!NT_SUCCESS(status)){ExFreePoolWithTag(s,TAG);return status;}
 s->finalize=IoAllocateWorkItem(owner);if(!s->finalize){ObDereferenceObject(s->file);ExFreePoolWithTag(s,TAG);return STATUS_INSUFFICIENT_RESOURCES;}
 s->owner=owner;ObReferenceObject(owner);
 s->provider=*p;s->provider.reference(s->provider.context);
 status=s->provider.match_hfp_target(s->provider.context,s->target);
 if(!NT_SUCCESS(status)){s->provider.dereference(s->provider.context);IoFreeWorkItem(s->finalize);ObDereferenceObject(s->owner);ObDereferenceObject(s->file);ExFreePoolWithTag(s,TAG);return status;}
 s->refs=1;KeInitializeEvent(&s->serial,SynchronizationEvent,TRUE);*out=s;return STATUS_SUCCESS;
}
NTSTATUS a04_bt_hfp_register_notifications(PDRIVER_OBJECT driver,PDRIVER_NOTIFICATION_CALLBACK_ROUTINE callback,PVOID context,PVOID *registration)
{
 if(!driver||!callback||!registration||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 return IoRegisterPlugPlayNotification(EventCategoryDeviceInterfaceChange,PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES,
  (PVOID)&GUID_DEVINTERFACE_BLUETOOTH_HFP_SCO_HCIBYPASS,driver,callback,context,registration);
}
NTSTATUS a04_bt_hfp_unregister_notifications(PVOID registration)
{return KeGetCurrentIrql()==PASSIVE_LEVEL&&registration?IoUnregisterPlugPlayNotificationEx(registration):STATUS_INVALID_PARAMETER;}
