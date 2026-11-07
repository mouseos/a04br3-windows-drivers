/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT6630 BTHX bus client. No MMIO, interrupt, firmware, NVRAM or power owner.
 * Only the NDIS broker executes SDIO. Root-enumerated test profile is separate
 * from the physical Wi-Fi child; no legacy BT parent profile is selected. */
#include <ntddk.h>
#include <wdf.h>
#include <bthxddi.h>
#include "broker_abi.h"
#include "radio_binding.h"
#define CONTEXT_BYTES FIELD_OFFSET(BTHX_HCI_READ_WRITE_CONTEXT,Data)
C_ASSERT(sizeof(BTHX_VERSION)==4);
C_ASSERT(sizeof(BTHX_CAPABILITIES)==16);
C_ASSERT(CONTEXT_BYTES==5);
typedef struct _CLIENT {
 WDFDEVICE device,child;WDFIOTARGET target;WDFWAITLOCK lock;WDFTIMER timer;WDFQUEUE reads[2];
 struct a04_mt6630_radio_binding sideband;
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
static BOOLEAN SidebandCapable(CLIENT *c)
{
 struct a04_bt_broker_state state={0};WDF_MEMORY_DESCRIPTOR buffer;ULONG_PTR used=0;NTSTATUS s;
 WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&buffer,&state,sizeof(state));lock(c);
 if(!c->running||!c->open){unlock(c);return FALSE;}
 s=WdfIoTargetSendIoctlSynchronously(c->target,WDF_NO_HANDLE,A04_BT_BROKER_GET_STATE,NULL,&buffer,NULL,&used);
 unlock(c);
 if(!NT_SUCCESS(s)||used!=sizeof(state)||state.version!=1||!state.ready||!state.pcm_command_applied)return FALSE;
 /* Actual provider callbacks execute outside the transport lock: controller
  * ownership validation may itself acquire that lock. No active peer gate. */
 if(!a04_mt6630_binding_capable(&c->sideband))return FALSE;
 lock(c);BOOLEAN ready=c->running&&c->open;unlock(c);return ready;
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
VOID Timer(WDFTIMER timer)
{
 CLIENT *c=Context(WdfTimerGetParentObject(timer));lock(c);
 if(c->running){
  NTSTATUS s=STATUS_SUCCESS;
  /* Do not destructively read from the broker when our bounded queue is full.
   * The broker retains its own bounded queue, with overflow a real fault. */
  if(c->count<32){
   s=call(c,A04_BT_BROKER_READ,&c->scratch);
   if(NT_SUCCESS(s)){
    if(c->scratch.version!=1||(c->scratch.type!=2&&c->scratch.type!=4)||!c->scratch.length||c->scratch.length>sizeof(c->scratch.data))s=STATUS_DATA_ERROR;
    else c->packets[c->count++]=c->scratch;
   }
   RtlSecureZeroMemory(&c->scratch,sizeof(c->scratch));
  }
  if(NT_SUCCESS(s)||s==STATUS_NO_MORE_ENTRIES){deliver(c);WdfTimerStart(timer,WDF_REL_TIMEOUT_IN_MS(5));}
  else {c->running=FALSE;WdfIoQueuePurge(c->reads[0],NULL,NULL);WdfIoQueuePurge(c->reads[1],NULL,NULL);}
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
   if(SidebandCapable(c)){caps->ScoSupport=ScoSupportHCIBypass;caps->MaxScoChannels=1;}
   else caps->ScoSupport=ScoSupportNone;
   used=sizeof(*caps);
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
 UNREFERENCED_PARAMETER(previous);lock(c);
 WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&o,&name,GENERIC_READ|GENERIC_WRITE);o.ShareAccess=FILE_SHARE_READ|FILE_SHARE_WRITE;
 s=WdfIoTargetOpen(c->target,&o);if(!NT_SUCCESS(s))goto done;c->open=TRUE;
 s=call(c,A04_BT_BROKER_OPEN,NULL);if(!NT_SUCCESS(s)){WdfIoTargetClose(c->target);c->open=FALSE;goto done;}
 c->running=TRUE;WdfIoQueueStart(c->reads[0]);WdfIoQueueStart(c->reads[1]);WdfTimerStart(c->timer,WDF_REL_TIMEOUT_IN_MS(5));
done:unlock(c);return s;
}
NTSTATUS Stop(WDFDEVICE device,WDF_POWER_DEVICE_STATE target)
{
 CLIENT *c=Context(device);NTSTATUS s=STATUS_SUCCESS;UNREFERENCED_PARAMETER(target);
 /* Audio must first remove its HFP endpoints and prove both DMA directions
  * quiescent. Keep transport/clock ownership while the binding remains. */
 lock(c);if(c->binding_active){unlock(c);return STATUS_DEVICE_BUSY;}c->running=FALSE;unlock(c);WdfTimerStop(c->timer,TRUE);
 WdfIoQueuePurgeSynchronously(c->reads[0]);WdfIoQueuePurgeSynchronously(c->reads[1]);lock(c);
 if(c->open){s=call(c,A04_BT_BROKER_CLOSE,NULL);WdfIoTargetClose(c->target);c->open=FALSE;}
 RtlSecureZeroMemory(c->packets,sizeof(c->packets));c->count=0;unlock(c);return s;
}
static NTSTATUS child(CLIENT *c)
{
 PWDFDEVICE_INIT init=WdfPdoInitAllocate(c->device);WDF_OBJECT_ATTRIBUTES a;WDF_IO_QUEUE_CONFIG q;NTSTATUS s;UNICODE_STRING id;
 if(!init)return STATUS_INSUFFICIENT_RESOURCES;WdfPdoInitAllowForwardingRequestToParent(init);
 RtlInitUnicodeString(&id,L"A04BR3\\MT6630HCI");s=WdfPdoInitAssignDeviceID(init,&id);if(!NT_SUCCESS(s))goto fail;
 s=WdfPdoInitAddHardwareID(init,&id);if(!NT_SUCCESS(s))goto fail;
 RtlInitUnicodeString(&id,L"MS_BTHX_BTHMINI");s=WdfPdoInitAddCompatibleID(init,&id);if(!NT_SUCCESS(s))goto fail;
 RtlInitUnicodeString(&id,L"0");s=WdfPdoInitAssignInstanceID(init,&id);if(!NT_SUCCESS(s))goto fail;
 WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ExecutionLevel=WdfExecutionLevelPassive;s=WdfDeviceCreate(&init,&a,&c->child);if(!NT_SUCCESS(s))goto fail;
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchParallel);q.PowerManaged=WdfFalse;q.EvtIoDeviceControl=Forward;q.EvtIoInternalDeviceControl=Forward;
 s=WdfIoQueueCreate(c->child,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);if(!NT_SUCCESS(s))return s;
 a04_mt6630_binding_init(&c->sideband,WdfDeviceWdmGetDeviceObject(c->child));
 struct a04_mt6630_sideband_interface iface;WDF_QUERY_INTERFACE_CONFIG query;RtlZeroMemory(&iface,sizeof(iface));
 iface.Header.Size=sizeof(iface);iface.Header.Version=A04_MT6630_SIDEBAND_VERSION;iface.Header.Context=c;
 iface.Header.InterfaceReference=RadioReference;iface.Header.InterfaceDereference=RadioDereference;
 iface.Bind=BindSideband;iface.Unbind=UnbindSideband;
 WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&iface,&A04Mt6630SidebandInterfaceGuid,NULL);
 s=WdfDeviceAddQueryInterface(c->child,&query);if(NT_SUCCESS(s))s=WdfFdoAddStaticChild(c->device,c->child);return s;
fail:WdfDeviceInitFree(init);return s;
}
NTSTATUS Add(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDFDEVICE d;CLIENT *c;WDF_OBJECT_ATTRIBUTES a;WDF_PNPPOWER_EVENT_CALLBACKS p;WDF_TIMER_CONFIG timer;WDF_IO_QUEUE_CONFIG q;NTSTATUS s;UNREFERENCED_PARAMETER(driver);
 WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&p);p.EvtDeviceD0Entry=Start;p.EvtDeviceD0Exit=Stop;WdfDeviceInitSetPnpPowerEventCallbacks(init,&p);
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&a,CLIENT);a.ExecutionLevel=WdfExecutionLevelPassive;
 s=WdfDeviceCreate(&init,&a,&d);if(!NT_SUCCESS(s))return s;c=Context(d);c->device=d;
 WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ParentObject=d;s=WdfWaitLockCreate(&a,&c->lock);if(!NT_SUCCESS(s))return s;
 s=WdfIoTargetCreate(d,WDF_NO_OBJECT_ATTRIBUTES,&c->target);if(!NT_SUCCESS(s))return s;
 WDF_TIMER_CONFIG_INIT(&timer,Timer);timer.AutomaticSerialization=FALSE;a.ExecutionLevel=WdfExecutionLevelPassive;
 s=WdfTimerCreate(&timer,&a,&c->timer);if(!NT_SUCCESS(s))return s;
 for(unsigned i=0;i<2;i++){WDF_IO_QUEUE_CONFIG_INIT(&q,WdfIoQueueDispatchManual);q.PowerManaged=WdfFalse;s=WdfIoQueueCreate(d,&q,WDF_NO_OBJECT_ATTRIBUTES,&c->reads[i]);if(!NT_SUCCESS(s))return s;}
 WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&q,WdfIoQueueDispatchParallel);q.PowerManaged=WdfFalse;q.EvtIoDeviceControl=ComboInternal;q.EvtIoInternalDeviceControl=ComboInternal;
 s=WdfIoQueueCreate(d,&q,WDF_NO_OBJECT_ATTRIBUTES,WDF_NO_HANDLE);return NT_SUCCESS(s)?child(c):s;
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING path)
{WDF_DRIVER_CONFIG c;WDF_DRIVER_CONFIG_INIT(&c,Add);return WdfDriverCreate(object,path,WDF_NO_OBJECT_ATTRIBUTES,&c,WDF_NO_HANDLE);}
