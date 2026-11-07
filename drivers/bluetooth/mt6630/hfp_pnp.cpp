/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Real PortCls subdevice/physical connection and HFP notification lifecycle.
 * Exclusive MT6630 profile, one remote HFP pair at a time. No public broker.
 */
#include <initguid.h>
#include "hfp_pnp.h"
#include <wdmguid.h>
#define TAG 'p36M'
struct endpoint_set {
 struct a04_bt_hfp_session *session;PPORT ports[4];BOOLEAN registered[4],connected[2];
};
static WCHAR names[4][32]={L"MT6630SpeakerTopology",L"MT6630SpeakerWave",L"MT6630MicTopology",L"MT6630MicWave"};
static NTSTATUS unregister_set(PDEVICE_OBJECT device,struct endpoint_set *set)
{
 NTSTATUS status=a04_bt_hfp_detach(set->session);if(!NT_SUCCESS(status))return status;
 for(ULONG d=0;d<2;d++)if(set->connected[d]){
  PUNREGISTERPHYSICALCONNECTION remove=NULL;ULONG from=d?2:1,to=d?3:0;
  status=set->ports[from]->QueryInterface(IID_IUnregisterPhysicalConnection,(PVOID *)&remove);
  if(!NT_SUCCESS(status))return status;
  status=remove->UnregisterPhysicalConnection(device,set->ports[from],1,set->ports[to],d?1:0);
  remove->Release();if(!NT_SUCCESS(status))return status;set->connected[d]=FALSE;
 }
 for(LONG i=3;i>=0;i--)if(set->registered[i]){
  PUNREGISTERSUBDEVICE remove=NULL;
  status=set->ports[i]->QueryInterface(IID_IUnregisterSubdevice,(PVOID *)&remove);
  if(!NT_SUCCESS(status))return status;
  status=remove->UnregisterSubdevice(device,set->ports[i]);remove->Release();
  if(!NT_SUCCESS(status))return status;set->registered[i]=FALSE;
 }
 for(ULONG i=0;i<4;i++)if(set->ports[i])set->ports[i]->Release();
 a04_bt_hfp_dereference(set->session);ExFreePoolWithTag(set,TAG);return STATUS_SUCCESS;
}
/* *out survives any partial registration failure so teardown cannot lose it. */
static NTSTATUS register_set(PDEVICE_OBJECT device,PRESOURCELIST resources,
 struct a04_bt_hfp_session *session,struct endpoint_set **out)
{
 struct a04_bt_hfp_metadata metadata;NTSTATUS status=a04_bt_hfp_metadata(session,&metadata);*out=NULL;
 if(!NT_SUCCESS(status))return status;
 struct endpoint_set *set=(struct endpoint_set *)ExAllocatePoolWithTag(NonPagedPool,sizeof(*set),TAG);
 if(!set)return STATUS_INSUFFICIENT_RESOURCES;RtlZeroMemory(set,sizeof(*set));
 set->session=session;a04_bt_hfp_reference(session);*out=set;
 for(ULONG i=0;i<4;i++){
  PUNKNOWN miniport=NULL;
  if(i&1){PMINIPORTWAVERT wave=NULL;status=Mt6630NewHfpMiniport(session,i/2,&wave);miniport=wave;}
  else {PMINIPORTTOPOLOGY topo=NULL;status=Mt6630NewHfpTopology(session,i/2,&metadata,&topo);miniport=topo;}
  if(!NT_SUCCESS(status))return status;
  status=PcNewPort(&set->ports[i],(i&1)?CLSID_PortWaveRT:CLSID_PortTopology);
  if(NT_SUCCESS(status))status=set->ports[i]->Init(device,NULL,miniport,NULL,resources);
  miniport->Release();if(!NT_SUCCESS(status))return status;
  /* Verify teardown support BEFORE publishing each subdevice. */
  PUNREGISTERSUBDEVICE remove=NULL;
  status=set->ports[i]->QueryInterface(IID_IUnregisterSubdevice,(PVOID *)&remove);
  if(!NT_SUCCESS(status))return status;remove->Release();
  status=PcRegisterSubdevice(device,names[i],set->ports[i]);if(!NT_SUCCESS(status))return status;set->registered[i]=TRUE;
 }
 /* speaker wave bridge1 -> topology input0; mic topology output1 -> wave bridge1 */
 for(ULONG d=0;d<2;d++){
  ULONG from=d?2:1,to=d?3:0;PUNREGISTERPHYSICALCONNECTION remove=NULL;
  status=set->ports[from]->QueryInterface(IID_IUnregisterPhysicalConnection,(PVOID *)&remove);
  if(!NT_SUCCESS(status))return status;remove->Release();
  status=PcRegisterPhysicalConnection(device,set->ports[from],1,set->ports[to],d?1:0);
  if(!NT_SUCCESS(status))return status;set->connected[d]=TRUE;
 }
 return STATUS_SUCCESS;
}
struct notification {LIST_ENTRY entry;BOOLEAN arrival;UNICODE_STRING link;WCHAR text[1];};
struct mt6630_hfp_pnp {
 PDEVICE_OBJECT device;PRESOURCELIST resources;struct a04_bt_hfp_provider provider;
 PIO_WORKITEM work;PVOID registration;LIST_ENTRY pending;KSPIN_LOCK lock;KEVENT idle;
 BOOLEAN scheduled;volatile LONG stopping,failure;ULONG queued;
 struct a04_bt_hfp_session *session;struct endpoint_set *endpoints;UNICODE_STRING link;
};
static void fault(struct mt6630_hfp_pnp *m,NTSTATUS status)
{InterlockedExchange(&m->failure,(LONG)status);m->provider.fault(m->provider.context,status);}
static NTSTATUS remove_current(struct mt6630_hfp_pnp *m)
{
 NTSTATUS status=STATUS_SUCCESS;
 if(m->endpoints){status=unregister_set(m->device,m->endpoints);if(!NT_SUCCESS(status))return status;m->endpoints=NULL;}
 if(m->session){status=a04_bt_hfp_detach(m->session);if(!NT_SUCCESS(status))return status;a04_bt_hfp_dereference(m->session);m->session=NULL;}
 if(m->link.Buffer){RtlSecureZeroMemory(m->link.Buffer,m->link.MaximumLength);ExFreePoolWithTag(m->link.Buffer,TAG);RtlZeroMemory(&m->link,sizeof(m->link));}
 return status;
}
static void arrival(struct mt6630_hfp_pnp *m,PUNICODE_STRING link)
{
 if(m->session)return; /* This exclusive profile supports one live HFP pair. */
 /* Own a private runtime link, never print it or put it in an artifact. */
 PWSTR saved=(PWSTR)ExAllocatePoolWithTag(NonPagedPool,link->Length+sizeof(WCHAR),TAG);
 if(!saved){fault(m,STATUS_INSUFFICIENT_RESOURCES);return;}
 RtlZeroMemory(saved,link->Length+sizeof(WCHAR));RtlCopyMemory(saved,link->Buffer,link->Length);
 NTSTATUS status=a04_bt_hfp_attach(m->device,link,&m->provider,&m->session);
 if(!NT_SUCCESS(status)){RtlSecureZeroMemory(saved,link->Length+sizeof(WCHAR));ExFreePoolWithTag(saved,TAG);
  /* Unrelated radio or an interface removed during arrival is not our fault. */
  if(status!=STATUS_NOT_SUPPORTED&&status!=STATUS_NO_SUCH_DEVICE&&status!=STATUS_DEVICE_REMOVED&&status!=STATUS_DEVICE_NOT_CONNECTED)fault(m,status);
  return;}
 m->link.Buffer=saved;m->link.Length=link->Length;m->link.MaximumLength=link->Length+sizeof(WCHAR);
 status=register_set(m->device,m->resources,m->session,&m->endpoints);
 if(!NT_SUCCESS(status)){fault(m,status);NTSTATUS cleanup=remove_current(m);if(!NT_SUCCESS(cleanup))fault(m,cleanup);}
}
static void worker(PDEVICE_OBJECT device,PVOID context)
{
 struct mt6630_hfp_pnp *m=(struct mt6630_hfp_pnp *)context;UNREFERENCED_PARAMETER(device);
 for(;;){
  KIRQL irql;KeAcquireSpinLock(&m->lock,&irql);
  if(IsListEmpty(&m->pending)){
   m->scheduled=FALSE;KeSetEvent(&m->idle,IO_NO_INCREMENT,FALSE);KeReleaseSpinLock(&m->lock,irql);return;
  }
  struct notification *n=CONTAINING_RECORD(RemoveHeadList(&m->pending),struct notification,entry);m->queued--;
  KeReleaseSpinLock(&m->lock,irql);
  if(!InterlockedCompareExchange(&m->stopping,0,0)&&!InterlockedCompareExchange(&m->failure,0,0)){
   if(n->arrival)arrival(m,&n->link);
   else if(m->session&&RtlEqualUnicodeString(&m->link,&n->link,TRUE)){
    NTSTATUS status=remove_current(m);if(!NT_SUCCESS(status))fault(m,status);
   }
  }
  RtlSecureZeroMemory(n->text,n->link.MaximumLength);ExFreePoolWithTag(n,TAG);
 }
}
static NTSTATUS notify(PVOID event,PVOID context)
{
 struct mt6630_hfp_pnp *m=(struct mt6630_hfp_pnp *)context;
 PDEVICE_INTERFACE_CHANGE_NOTIFICATION change=(PDEVICE_INTERFACE_CHANGE_NOTIFICATION)event;
 BOOLEAN arrived=IsEqualGUIDAligned(change->Event,GUID_DEVICE_INTERFACE_ARRIVAL);
 if(!arrived&&!IsEqualGUIDAligned(change->Event,GUID_DEVICE_INTERFACE_REMOVAL))return STATUS_SUCCESS;
 if(InterlockedCompareExchange(&m->stopping,0,0))return STATUS_SUCCESS;
 PUNICODE_STRING link=change->SymbolicLinkName;
 if(!link||!link->Buffer||!link->Length||(link->Length&1)||link->Length>4096){fault(m,STATUS_INVALID_PARAMETER);return STATUS_SUCCESS;}
 SIZE_T bytes=FIELD_OFFSET(struct notification,text)+link->Length+sizeof(WCHAR);
 struct notification *n=(struct notification *)ExAllocatePoolWithTag(NonPagedPool,bytes,TAG);
 if(!n){fault(m,STATUS_INSUFFICIENT_RESOURCES);return STATUS_SUCCESS;}RtlZeroMemory(n,bytes);
 n->arrival=arrived;n->link.Buffer=n->text;n->link.Length=link->Length;n->link.MaximumLength=link->Length+sizeof(WCHAR);
 RtlCopyMemory(n->text,link->Buffer,link->Length);
 KIRQL irql;BOOLEAN queue=FALSE,overflow=FALSE,stopped=FALSE;KeAcquireSpinLock(&m->lock,&irql);
 if(m->queued>=64||m->stopping||m->failure){overflow=TRUE;stopped=m->stopping||m->failure;}
 else {InsertTailList(&m->pending,&n->entry);m->queued++;
  if(!m->scheduled){m->scheduled=TRUE;KeClearEvent(&m->idle);queue=TRUE;}}
 KeReleaseSpinLock(&m->lock,irql);
 if(overflow){RtlSecureZeroMemory(n->text,n->link.MaximumLength);ExFreePoolWithTag(n,TAG);if(!stopped)fault(m,STATUS_INSUFFICIENT_RESOURCES);}
 else if(queue)IoQueueWorkItem(m->work,worker,DelayedWorkQueue,m);
 return STATUS_SUCCESS;
}
NTSTATUS Mt6630HfpPnpStart(PDEVICE_OBJECT device,PRESOURCELIST resources,
 const struct a04_bt_hfp_provider *p,struct mt6630_hfp_pnp **out)
{
 if(!out)return STATUS_INVALID_PARAMETER;*out=NULL;
 if(!device||!resources||!p||KeGetCurrentIrql()!=PASSIVE_LEVEL||p->size!=sizeof(*p)||p->version!=A04_BT_HFP_PROVIDER_VERSION||
    !p->reference||!p->dereference||!p->fault||!p->match_hfp_target)return STATUS_INVALID_PARAMETER;
 struct mt6630_hfp_pnp *m=(struct mt6630_hfp_pnp *)ExAllocatePoolWithTag(NonPagedPool,sizeof(*m),TAG);
 if(!m)return STATUS_INSUFFICIENT_RESOURCES;RtlZeroMemory(m,sizeof(*m));
 m->work=IoAllocateWorkItem(device);if(!m->work){ExFreePoolWithTag(m,TAG);return STATUS_INSUFFICIENT_RESOURCES;}
 m->device=device;ObReferenceObject(device);m->resources=resources;resources->AddRef();m->provider=*p;p->reference(p->context);
 InitializeListHead(&m->pending);KeInitializeSpinLock(&m->lock);KeInitializeEvent(&m->idle,NotificationEvent,TRUE);
 *out=m;NTSTATUS status=a04_bt_hfp_register_notifications(device->DriverObject,notify,m,&m->registration);
 if(!NT_SUCCESS(status)){if(NT_SUCCESS(Mt6630HfpPnpStop(m)))*out=NULL;}
 return status;
}
NTSTATUS Mt6630HfpPnpStop(struct mt6630_hfp_pnp *m)
{
 if(!m||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
 InterlockedExchange(&m->stopping,1);
 if(m->registration){NTSTATUS status=a04_bt_hfp_unregister_notifications(m->registration);if(!NT_SUCCESS(status))return status;m->registration=NULL;}
 LARGE_INTEGER timeout;timeout.QuadPart=-100000000;
 NTSTATUS status=KeWaitForSingleObject(&m->idle,Executive,KernelMode,FALSE,&timeout);
 if(status!=STATUS_SUCCESS){fault(m,STATUS_IO_TIMEOUT);return STATUS_IO_TIMEOUT;}
 /* Notification unregistration drained callbacks. Acquire the gate after idle
  * to synchronize with the worker's final KeReleaseSpinLock before freeing. */
 KIRQL irql;KeAcquireSpinLock(&m->lock,&irql);BOOLEAN busy=m->scheduled||!IsListEmpty(&m->pending);KeReleaseSpinLock(&m->lock,irql);
 if(busy)return STATUS_DEVICE_BUSY;
 status=remove_current(m);if(!NT_SUCCESS(status)){fault(m,status);return status;}
 IoFreeWorkItem(m->work);m->resources->Release();ObDereferenceObject(m->device);m->provider.dereference(m->provider.context);
 RtlSecureZeroMemory(m,sizeof(*m));ExFreePoolWithTag(m,TAG);return STATUS_SUCCESS;
}
