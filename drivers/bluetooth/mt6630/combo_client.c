/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT6630 BTHX bus client. No MMIO, interrupt, firmware, NVRAM or power owner.
 * Only the NDIS broker executes SDIO. Root-enumerated test profile is separate
 * from the physical Wi-Fi child; no legacy BT parent profile is selected. */
#include <ntddk.h>
#include <wdf.h>
#include <bthxddi.h>
#include "broker_abi.h"
#include "radio_binding.h"
#include "combo_controller.h"
#include "rx_drain.h"
#include "transport_caps.h"
#include "start_status.h"
#include "start_checkpoint.h"
#define CONTEXT_BYTES FIELD_OFFSET(BTHX_HCI_READ_WRITE_CONTEXT,Data)
C_ASSERT(sizeof(BTHX_VERSION)==4);
C_ASSERT(sizeof(BTHX_CAPABILITIES)==16);
C_ASSERT(CONTEXT_BYTES==5);
typedef struct _CLIENT {
 WDFDEVICE device,child;WDFIOTARGET target;WDFWAITLOCK lock;WDFTIMER timer;WDFQUEUE reads[2];
 struct a04_mt6630_radio_binding sideband;struct a04_combo_controller controller;NTSTATUS controller_status;
 ULONG start_mask;
 BOOLEAN running,negotiated,open,binding_active;unsigned count;struct a04_bt_broker_packet packets[32],scratch;
} CLIENT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(CLIENT,Context)
EVT_WDF_DRIVER_DEVICE_ADD Add;
EVT_WDF_DEVICE_D0_ENTRY Start;
EVT_WDF_DEVICE_D0_EXIT Stop;
EVT_WDF_TIMER Timer;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL ComboInternal;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL Forward;
static void lock(CLIENT *c){(void)WdfWaitLockAcquire(c->lock,NULL);}
static void unlock(CLIENT *c){WdfWaitLockRelease(c->lock);}
static NTSTATUS call(CLIENT *c,ULONG code,struct a04_bt_broker_packet *packet)
{
 WDF_MEMORY_DESCRIPTOR buffer;ULONG_PTR used=0;NTSTATUS s;
 if(packet)WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&buffer,packet,sizeof(*packet));
 s=WdfIoTargetSendIoctlSynchronously(c->target,WDF_NO_HANDLE,code,
  code==A04_BT_BROKER_WRITE?&buffer:NULL,code==A04_BT_BROKER_READ?&buffer:NULL,NULL,&used);
 if(NT_SUCCESS(s)&&used!=(code==A04_BT_BROKER_READ?sizeof(*packet):0))return STATUS_DATA_ERROR;
 return s;
}
static void RadioReference(PVOID p){CLIENT *c=p;WdfObjectReference(c->device);}
static void RadioDereference(PVOID p){CLIENT *c=p;WdfObjectDereference(c->device);}
static NTSTATUS BindSideband(PVOID p,const struct a04_bt_hfp_provider *provider,PULONG generation)
{
 CLIENT *c=p;NTSTATUS s=WdfDeviceStopIdle(c->device,TRUE);if(!NT_SUCCESS(s))return s;lock(c);
 if(!c->running||c->binding_active){unlock(c);WdfDeviceResumeIdle(c->device);return STATUS_DEVICE_NOT_READY;}
 /* Reserve D0 before callbacks. Stop consults this gate without acquiring
  * the binding mutex, so provider/controller callbacks may take c->lock. */
 c->binding_active=TRUE;unlock(c);
 s=a04_mt6630_binding_bind(&c->sideband,provider,generation);
 if(!NT_SUCCESS(s)){lock(c);c->binding_active=FALSE;unlock(c);WdfDeviceResumeIdle(c->device);}return s;
}
static NTSTATUS UnbindSideband(PVOID p,ULONG generation)
{
 CLIENT *c=p;NTSTATUS s=a04_mt6630_binding_unbind(&c->sideband,generation);
 if(NT_SUCCESS(s)){lock(c);c->binding_active=FALSE;unlock(c);WdfDeviceResumeIdle(c->device);}return s;
}
static NTSTATUS TransportBypassStatus(CLIENT *c)
{
 struct a04_bt_transport_caps state={0};WDF_MEMORY_DESCRIPTOR buffer;ULONG_PTR used=0;NTSTATUS s;
 WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&buffer,&state,sizeof(state));lock(c);
 if(!c->running||!c->open){s=STATUS_DEVICE_NOT_READY;goto done;}
 s=WdfIoTargetSendIoctlSynchronously(c->target,WDF_NO_HANDLE,A04_BT_BROKER_TRANSPORT_CAPS,NULL,&buffer,NULL,&used);
 if(!NT_SUCCESS(s))goto done;
 if(used!=sizeof(state)||state.version!=1||state.ready>1){s=STATUS_DATA_ERROR;goto done;}
 if(!state.ready){s=STATUS_DEVICE_NOT_READY;goto done;}
 if(!a04_mt6630_transport_bypass(&state))s=STATUS_DATA_ERROR;
done:
 StartCheckpoint(c->device,&c->start_mask,15,s);unlock(c);return s;
}
static void RecordBrokerOpen(CLIENT *c,NTSTATUS actual_open_status)
{
 struct a04_bt_start_status receipt={0};WDF_MEMORY_DESCRIPTOR buffer;ULONG_PTR used=0;NTSTATUS s;
 WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&buffer,&receipt,sizeof(receipt));
 s=WdfIoTargetSendIoctlSynchronously(c->target,WDF_NO_HANDLE,A04_BT_BROKER_START_STATUS,NULL,&buffer,NULL,&used);
 if(NT_SUCCESS(s)&&used==sizeof(receipt)&&receipt.version==1&&receipt.bytes==sizeof(receipt)&&receipt.sequence&&
    (receipt.flags&A04_BT_START_RETURNED)&&(receipt.flags&A04_BT_START_NT_VALID)&&
    receipt.broker_ntstatus==(ULONG)actual_open_status&&!(receipt.entered_mask&~0x1fffeu)&&
    !(receipt.returned_mask&~receipt.entered_mask)&&!(receipt.succeeded_mask&~receipt.returned_mask)&&
    receipt.last_stage<=A04_BT_START_READY)BrokerStartCheckpoint(c->device,&receipt);
 else if(NT_SUCCESS(s))s=STATUS_DATA_ERROR;
 StartCheckpoint(c->device,&c->start_mask,16,s);
 /* Unknown/old provider or failed observation never changes the real OPEN. */
 RtlSecureZeroMemory(&receipt,sizeof(receipt));
}
static void deliver(CLIENT *c)
{
 for(unsigned i=0;i<c->count;){
  struct a04_bt_broker_packet *p=&c->packets[i];WDFREQUEST r;PVOID out;size_t bytes;
  if(!NT_SUCCESS(WdfIoQueueRetrieveNextRequest(c->reads[p->type==4?0:1],&r))){i++;continue;}
  NTSTATUS s=WdfRequestRetrieveOutputBuffer(r,CONTEXT_BYTES+p->length,&out,&bytes);
  if(!NT_SUCCESS(s)){WdfRequestComplete(r,s);continue;}
  PBTHX_HCI_READ_WRITE_CONTEXT h=out;h->Type=(UCHAR)p->type;h->DataLen=p->length;RtlCopyMemory(h->Data,p->data,p->length);
  WdfRequestCompleteWithInformation(r,STATUS_SUCCESS,CONTEXT_BYTES+p->length);
  for(unsigned j=i+1;j<c->count;j++)c->packets[j-1]=c->packets[j];
  RtlSecureZeroMemory(&c->packets[--c->count],sizeof(*p));
 }
}
struct RX_DRAIN {CLIENT *client;NTSTATUS status;};
static void RxDeliver(void *p){deliver(((struct RX_DRAIN*)p)->client);}
static int RxRoom(void *p){return ((struct RX_DRAIN*)p)->client->count<32;}
static int RxPull(void *p)
{
 struct RX_DRAIN *drain=p;CLIENT *c=drain->client;
 drain->status=call(c,A04_BT_BROKER_READ,&c->scratch);
 if(NT_SUCCESS(drain->status)){
  if(c->scratch.version!=1||(c->scratch.type!=2&&c->scratch.type!=4)||
     !c->scratch.length||c->scratch.length>sizeof(c->scratch.data))drain->status=STATUS_DATA_ERROR;
  else c->packets[c->count++]=c->scratch;
 }
 RtlSecureZeroMemory(&c->scratch,sizeof(c->scratch));
 return NT_SUCCESS(drain->status)?1:drain->status==STATUS_NO_MORE_ENTRIES?0:-1;
}
VOID Timer(WDFTIMER timer)
{
 CLIENT *c=Context(WdfTimerGetParentObject(timer));lock(c);
 if(c->running){
  struct RX_DRAIN state={c,STATUS_SUCCESS};
  const struct mt6630_rx_drain_io io={&state,RxDeliver,RxRoom,RxPull};
  /* CT3 STP worker drains pending RX. Keep the existing single SDIO worker
   * and finite queues: at most32 packets, stop on empty/full/real error.
   * No destructive broker read when pending BTHX requests cannot make room. */
  if(mt6630_rx_drain(&io)!=MT6630_RX_FAULT)WdfTimerStart(timer,WDF_REL_TIMEOUT_IN_MS(5));
  else {InterlockedExchange(&c->controller.uncertain,1);c->running=FALSE;WdfIoQueuePurge(c->reads[0],NULL,NULL);WdfIoQueuePurge(c->reads[1],NULL,NULL);}
 }
 unlock(c);
}
VOID ComboInternal(WDFQUEUE q,WDFREQUEST r,size_t outlen,size_t inlen,ULONG code)
{
 CLIENT *c=Context(WdfIoQueueGetDevice(q));PVOID in,out;size_t bytes;NTSTATUS s=STATUS_INVALID_PARAMETER;ULONG_PTR used=0;
 UNREFERENCED_PARAMETER(outlen);UNREFERENCED_PARAMETER(inlen);
 if(WdfRequestGetRequestorMode(r)!=KernelMode){WdfRequestComplete(r,STATUS_ACCESS_DENIED);return;}
 if(code==IOCTL_BTHX_QUERY_CAPABILITIES){
  s=WdfRequestRetrieveOutputBuffer(r,sizeof(BTHX_CAPABILITIES),&out,&bytes);
  if(NT_SUCCESS(s)){PBTHX_CAPABILITIES caps=out;RtlZeroMemory(caps,sizeof(*caps));
   caps->MaxAclTransferInSize=A04_BT_BROKER_MAX_HCI;
   s=TransportBypassStatus(c);
   if(NT_SUCCESS(s)){caps->ScoSupport=ScoSupportHCIBypass;caps->MaxScoChannels=1;used=sizeof(*caps);}
   /* Failure is an actual NTSTATUS, not a successful None/0 capability. */
  }
  WdfRequestCompleteWithInformation(r,s,used);return;
 }
 lock(c);
 switch(code){
 case IOCTL_BTHX_GET_VERSION:
  s=WdfRequestRetrieveOutputBuffer(r,sizeof(BTHX_VERSION),&out,&bytes);
  if(NT_SUCCESS(s)){((PBTHX_VERSION)out)->Version=BTHX_DDI_VERSION_1;used=sizeof(BTHX_VERSION);}break;
 case IOCTL_BTHX_SET_VERSION:
  s=WdfRequestRetrieveInputBuffer(r,sizeof(BTHX_VERSION),&in,&bytes);
  if(NT_SUCCESS(s)){s=((PBTHX_VERSION)in)->Version==BTHX_DDI_VERSION_1?STATUS_SUCCESS:STATUS_REVISION_MISMATCH;c->negotiated=NT_SUCCESS(s);}break;
 case IOCTL_BTHX_WRITE_HCI:
  if(!c->running||!c->negotiated){s=STATUS_DEVICE_NOT_READY;break;}
  s=WdfRequestRetrieveOutputBuffer(r,sizeof(ULONG),&out,&bytes);if(!NT_SUCCESS(s))break;
  s=WdfRequestRetrieveInputBuffer(r,CONTEXT_BYTES+1,&in,&bytes);if(!NT_SUCCESS(s))break;
  {PBTHX_HCI_READ_WRITE_CONTEXT h=in;
   if(!h->DataLen||h->DataLen>sizeof(c->scratch.data)||h->DataLen>bytes-CONTEXT_BYTES){s=STATUS_INVALID_BUFFER_SIZE;break;}
   c->scratch.version=1;c->scratch.type=h->Type;c->scratch.length=h->DataLen;RtlCopyMemory(c->scratch.data,h->Data,h->DataLen);
   s=call(c,A04_BT_BROKER_WRITE,&c->scratch);RtlSecureZeroMemory(&c->scratch,sizeof(c->scratch));
   if(NT_SUCCESS(s)){*(ULONG *)out=h->DataLen;used=sizeof(ULONG);}
  }break;
 case IOCTL_BTHX_READ_HCI:
  if(!c->running||!c->negotiated){s=STATUS_DEVICE_NOT_READY;break;}
  s=WdfRequestRetrieveInputBuffer(r,sizeof(BTHX_HCI_PACKET_TYPE),&in,&bytes);if(!NT_SUCCESS(s))break;
  {unsigned type=*(BTHX_HCI_PACKET_TYPE *)in;
   if(type!=2&&type!=4){s=STATUS_INVALID_PARAMETER;break;}
   s=WdfRequestRetrieveOutputBuffer(r,CONTEXT_BYTES+1,&out,&bytes);if(!NT_SUCCESS(s))break;
   s=WdfRequestForwardToIoQueue(r,c->reads[type==4?0:1]);if(NT_SUCCESS(s)){deliver(c);unlock(c);return;}
  }break;
 default:s=STATUS_NOT_SUPPORTED;
 }
 unlock(c);WdfRequestCompleteWithInformation(r,s,used);
}
VOID Forward(WDFQUEUE q,WDFREQUEST r,size_t outlen,size_t inlen,ULONG code)
{
 UNREFERENCED_PARAMETER(outlen);UNREFERENCED_PARAMETER(inlen);
 if(WdfRequestGetRequestorMode(r)!=KernelMode){WdfRequestComplete(r,STATUS_ACCESS_DENIED);return;}
 switch(code){case IOCTL_BTHX_GET_VERSION:case IOCTL_BTHX_SET_VERSION:case IOCTL_BTHX_QUERY_CAPABILITIES:case IOCTL_BTHX_WRITE_HCI:case IOCTL_BTHX_READ_HCI:break;
 default:WdfRequestComplete(r,STATUS_NOT_SUPPORTED);return;}
 WDF_REQUEST_FORWARD_OPTIONS o;WDF_REQUEST_FORWARD_OPTIONS_INIT(&o);o.Flags=WDF_REQUEST_FORWARD_OPTION_SEND_AND_FORGET;
 NTSTATUS s=WdfRequestForwardToParentDeviceIoQueue(r,WdfDeviceGetDefaultQueue(WdfPdoGetParent(WdfIoQueueGetDevice(q))),&o);
 if(!NT_SUCCESS(s))WdfRequestComplete(r,s);
}
NTSTATUS Start(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
 CLIENT *c=Context(device);NTSTATUS s;WDF_IO_TARGET_OPEN_PARAMS o;UNICODE_STRING name=RTL_CONSTANT_STRING(A04_BT_BROKER_NAME);
 UNREFERENCED_PARAMETER(previous);lock(c);c->start_mask&=(1u<<9)-1u;StartCheckpoint(device,&c->start_mask,9,STATUS_SUCCESS);
 WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&o,&name,GENERIC_READ|GENERIC_WRITE);o.ShareAccess=FILE_SHARE_READ|FILE_SHARE_WRITE;
 s=WdfIoTargetOpen(c->target,&o);StartCheckpoint(device,&c->start_mask,10,s);if(!NT_SUCCESS(s))goto done;c->open=TRUE;
 s=call(c,A04_BT_BROKER_OPEN,NULL);StartCheckpoint(device,&c->start_mask,11,s);RecordBrokerOpen(c,s);if(!NT_SUCCESS(s)){WdfIoTargetClose(c->target);c->open=FALSE;goto done;}
 c->running=TRUE;
 /* Optional controller registration is separate from a real raw HCI OPEN.
  * An absent/failed interface never grants an Audio controller lease;
  * transport BTHX bypass caps retain the separate real broker predicate. */
 c->controller_status=a04_combo_controller_start_locked(&c->controller);
 StartCheckpoint(device,&c->start_mask,12,c->controller_status);
 WdfIoQueueStart(c->reads[0]);WdfIoQueueStart(c->reads[1]);WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(5));
done:StartCheckpoint(device,&c->start_mask,13,s);unlock(c);return s;
}
NTSTATUS Stop(WDFDEVICE device,WDF_POWER_DEVICE_STATE target)
{
 CLIENT *c=Context(device);NTSTATUS s=STATUS_SUCCESS;UNREFERENCED_PARAMETER(target);
 /* Audio must first remove its HFP endpoints and prove both DMA directions
  * quiescent. Keep transport/clock ownership while the binding remains. */
 lock(c);if(c->binding_active){StartCheckpoint(device,&c->start_mask,14,STATUS_DEVICE_BUSY);unlock(c);return STATUS_DEVICE_BUSY;}
 s=a04_combo_controller_stop_locked(&c->controller);if(!NT_SUCCESS(s)){StartCheckpoint(device,&c->start_mask,14,s);unlock(c);return s;}c->running=FALSE;unlock(c);WdfTimerStop(c->timer,TRUE);
 WdfIoQueuePurgeSynchronously(c->reads[0]);WdfIoQueuePurgeSynchronously(c->reads[1]);lock(c);
 if(c->open){s=call(c,A04_BT_BROKER_CLOSE,NULL);WdfIoTargetClose(c->target);c->open=FALSE;}
 RtlSecureZeroMemory(c->packets,sizeof(c->packets));c->count=0;StartCheckpoint(device,&c->start_mask,14,s);unlock(c);return s;
}
static NTSTATUS child(CLIENT *c)
{
 PWDFDEVICE_INIT init=WdfPdoInitAllocate(c->device);WDF_OBJECT_ATTRIBUTES a;WDF_IO_QUEUE_CONFIG q;NTSTATUS s;UNICODE_STRING id;
 if(!init)return STATUS_INSUFFICIENT_RESOURCES;WdfPdoInitAllowForwardingRequestToParent(init);
 RtlInitUnicodeString(&id,L"A04BR3\\MT6630HCI");s=WdfPdoInitAssignDeviceID(init,&id);if(!NT_SUCCESS(s))goto fail;
 s=WdfPdoInitAddHardwareID(init,&id);if(!NT_SUCCESS(s))goto fail;
 RtlInitUnicodeString(&id,L"MS_BTHX_BTHMINI");s=WdfPdoInitAddCompatibleID(init,&id);if(!NT_SUCCESS(s))goto fail;
 RtlInitUnicodeString(&id,L"0");s=WdfPdoInitAssignInstanceID(init,&id);if(!NT_SUCCESS(s))goto fail;
 WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ExecutionLevel=WdfExecutionLevelPassive;s=WdfDeviceCreate(&init,&a,&c->child);StartCheckpoint(c->device,&c->start_mask,5,s);if(!NT_SUCCESS(s))goto fail;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchParallel);q.PowerManaged=WdfFalse;q.EvtIoDeviceControl=Forward;q.EvtIoInternalDeviceControl=Forward;
 s=WdfIoQueueCreate(c->child,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);if(!NT_SUCCESS(s))return s;
 a04_mt6630_binding_init(&c->sideband,WdfDeviceWdmGetDeviceObject(c->child));
 struct a04_mt6630_sideband_interface iface;WDF_QUERY_INTERFACE_CONFIG query;RtlZeroMemory(&iface,sizeof(iface));
 iface.Header.Size=sizeof(iface);iface.Header.Version=A04_MT6630_SIDEBAND_VERSION;iface.Header.Context=c;
 iface.Header.InterfaceReference=RadioReference;iface.Header.InterfaceDereference=RadioDereference;
 iface.Bind=BindSideband;iface.Unbind=UnbindSideband;
 WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&iface,&A04Mt6630SidebandInterfaceGuid,NULL);
 s=WdfDeviceAddQueryInterface(c->child,&query);StartCheckpoint(c->device,&c->start_mask,6,s);if(!NT_SUCCESS(s))return s;
 s=a04_combo_controller_init(&c->controller,c->device,c->child,c->target,c->lock);
 StartCheckpoint(c->device,&c->start_mask,7,s);
 if(NT_SUCCESS(s)){s=WdfFdoAddStaticChild(c->device,c->child);StartCheckpoint(c->device,&c->start_mask,8,s);}return s;
fail:WdfDeviceInitFree(init);return s;
}
/* Root removal is independent of physical parent removal. Revoke liveness
 * even while uncertain SCO references deliberately retain object storage. */
static VOID Surprise(WDFDEVICE device)
{
 CLIENT *c=Context(device);lock(c);InterlockedExchange(&c->controller.removed,1);c->running=FALSE;
 if(c->controller.ready&&NT_SUCCESS(IoSetDeviceInterfaceState(&c->controller.ready_link,FALSE)))c->controller.ready=FALSE;
 unlock(c);WdfIoQueuePurge(c->reads[0],NULL,NULL);WdfIoQueuePurge(c->reads[1],NULL,NULL);
}
static VOID Destroy(WDFOBJECT object){a04_combo_controller_destroy(&Context((WDFDEVICE)object)->controller);}
NTSTATUS Add(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDFDEVICE d;CLIENT *c;WDF_OBJECT_ATTRIBUTES a;WDF_PNPPOWER_EVENT_CALLBACKS p;WDF_TIMER_CONFIG timer;WDF_IO_QUEUE_CONFIG q;NTSTATUS s;UNREFERENCED_PARAMETER(driver);
 WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&p);p.EvtDeviceD0Entry=Start;p.EvtDeviceD0Exit=Stop;p.EvtDeviceSurpriseRemoval=Surprise;WdfDeviceInitSetPnpPowerEventCallbacks(init,&p);
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a,CLIENT);a.EvtDestroyCallback=Destroy;a.ExecutionLevel=WdfExecutionLevelPassive;
 DriverStartCheckpoint(driver,L"AddEntered",STATUS_SUCCESS);s=WdfDeviceCreate(&init,&a,&d);DriverStartCheckpoint(driver,L"DeviceCreateStatus",s);if(!NT_SUCCESS(s))return s;c=Context(d);c->device=d;StartCheckpoint(d,&c->start_mask,0,STATUS_SUCCESS);
 WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ParentObject=d;s=WdfWaitLockCreate(&a,&c->lock);StartCheckpoint(d,&c->start_mask,1,s);if(!NT_SUCCESS(s))return s;
 s=WdfIoTargetCreate(d,WDF_NO_OBJECT_ATTRIBUTES,&c->target);StartCheckpoint(d,&c->start_mask,2,s);if(!NT_SUCCESS(s))return s;
 WDF_TIMER_CONFIG_INIT(&timer,Timer);timer.AutomaticSerialization=FALSE;a.ExecutionLevel=WdfExecutionLevelPassive;
 s=WdfTimerCreate(&timer,&a,&c->timer);StartCheckpoint(d,&c->start_mask,3,s);if(!NT_SUCCESS(s))return s;
 for(unsigned i=0;i<2;i++){WDF_IO_QUEUE_CONFIG_INIT(&q,WdfIoQueueDispatchManual);q.PowerManaged=WdfFalse;s=WdfIoQueueCreate(d,&q,WDF_NO_OBJECT_ATTRIBUTES,&c->reads[i]);if(!NT_SUCCESS(s))return s;}
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchParallel);q.PowerManaged=WdfFalse;q.EvtIoDeviceControl=ComboInternal;q.EvtIoInternalDeviceControl=ComboInternal;
 s=WdfIoQueueCreate(d,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);StartCheckpoint(d,&c->start_mask,4,s);return NT_SUCCESS(s)?child(c):s;
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING path)
{WDF_DRIVER_CONFIG c;WDFDRIVER driver;NTSTATUS s;WDF_DRIVER_CONFIG_INIT(&c,Add);s=WdfDriverCreate(object,path,WDF_NO_OBJECT_ATTRIBUTES,&c,&driver);if(NT_SUCCESS(s))DriverStartCheckpoint(driver,L"DriverCreated",s);return s;}
