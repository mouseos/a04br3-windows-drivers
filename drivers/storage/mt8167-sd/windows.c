/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * KMDF physical host + persistent removable GenDisk PDO. Standard disk.sys
 * owns the disk class/partition/volume stack. No virtual Storport worker whose
 * extension or DMA common buffer can disappear underneath an outstanding SRB.
 */
#include <initguid.h>
#include "windows.h"
#include "checkpoint.h"
#include <acpiioct.h>
#include <wdmguid.h>
EVT_WDF_DRIVER_DEVICE_ADD SdAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE SdPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE SdRelease;
EVT_WDF_DEVICE_D0_ENTRY SdD0Entry;
EVT_WDF_DEVICE_D0_EXIT SdD0Exit;
EVT_WDF_DEVICE_SURPRISE_REMOVAL SdSurprise;
EVT_WDF_TIMER SdTimer;
static uint32_t read32(void *p,unsigned offset){struct sd_host *h=p;return READ_REGISTER_ULONG(h->registers+offset/4);}
static uint8_t read8(void *p,unsigned offset){struct sd_host *h=p;return READ_REGISTER_UCHAR((volatile UCHAR *)h->registers+offset);}
static void write32(void *p,unsigned offset,uint32_t value){struct sd_host *h=p;WRITE_REGISTER_ULONG(h->registers+offset/4,value);}
static uint64_t now_100ns(void *p)
{UNREFERENCED_PARAMETER(p);return KeQueryInterruptTime();}
static void delay_us(void *p,unsigned us){
 UNREFERENCED_PARAMETER(p);if(us<=50)KeStallExecutionProcessor(us);
 else {LARGE_INTEGER delay;delay.QuadPart=-(LONGLONG)us*10;(void)KeDelayExecutionThread(KernelMode,FALSE,&delay);}
}
static int live(void *p)
{
 struct sd_host *h=p;if(!h->registers)return 0;if(h->quiescing)return 1;
 return !InterlockedCompareExchange(&h->closing,0,0)&&InterlockedCompareExchange(&h->child_online,0,0)&&InterlockedCompareExchange(&h->present,0,0)==1&&
  h->run_generation==InterlockedCompareExchange(&h->generation,0,0)&&(!h->active_irp||!h->active_irp->Cancel);
}
NTSTATUS SdAcpi(struct sd_host *h,const char name[4],ULONG *value)
{
 ACPI_EVAL_INPUT_BUFFER input;UCHAR bytes[128];WDF_MEMORY_DESCRIPTOR in,out;WDF_REQUEST_SEND_OPTIONS options;
 PACPI_EVAL_OUTPUT_BUFFER result=(PACPI_EVAL_OUTPUT_BUFFER)bytes;ULONG_PTR returned=0;
 RtlZeroMemory(&input,sizeof(input));RtlZeroMemory(bytes,sizeof(bytes));input.Signature=ACPI_EVAL_INPUT_BUFFER_SIGNATURE;
 RtlCopyMemory(input.MethodName,name,4);WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&in,&input,sizeof(input));WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&out,bytes,sizeof(bytes));
 WDF_REQUEST_SEND_OPTIONS_INIT(&options,WDF_REQUEST_SEND_OPTION_TIMEOUT);options.Timeout=WDF_REL_TIMEOUT_IN_SEC(1);
 NTSTATUS status=WdfIoTargetSendIoctlSynchronously(WdfDeviceGetIoTarget(h->device),NULL,IOCTL_ACPI_EVAL_METHOD,&in,&out,&options,&returned);
 if(!NT_SUCCESS(status))return status;
 if(returned<FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER,Argument)+sizeof(ACPI_METHOD_ARGUMENT)||result->Signature!=ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE||
    result->Length>returned||result->Length<FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER,Argument)+sizeof(ACPI_METHOD_ARGUMENT)||result->Count!=1||
    result->Argument[0].Type!=ACPI_METHOD_ARGUMENT_INTEGER||result->Argument[0].DataLength<sizeof(ULONG))return STATUS_DEVICE_DATA_ERROR;
 *value=result->Argument[0].Argument;return STATUS_SUCCESS;
}
static void set_presence(struct sd_host *h,LONG state)
{
 LONG old=InterlockedExchange(&h->present,state);if(old==state)return;
 if(state==0){
  InterlockedExchange(&h->absent_seen,1);InterlockedExchange(&h->blocked,0);
  InterlockedIncrement(&h->generation);InterlockedExchange(&h->attention,1);InterlockedExchange(&h->ejected,0);
 }else if(state==1&&InterlockedExchange(&h->absent_seen,0)){
  InterlockedExchange(&h->blocked,0);InterlockedIncrement(&h->generation);InterlockedExchange(&h->attention,1);InterlockedExchange(&h->ejected,0);
 }
 /* An unknown/failed ACPI read aborts the current transaction via live(),
  * but recovery of that read is NOT proof of physical card reinsertion. */
}
VOID SdTimer(WDFTIMER timer)
{
 struct sd_host *h=SdHost(WdfTimerGetParentObject(timer));ULONG present=0;
 if(InterlockedCompareExchange(&h->closing,0,0))return;
 NTSTATUS status=SdPowerHealth(h);
 if(!NT_SUCCESS(status)){
  SdStopAdmission(h);InterlockedExchange(&h->blocked,1);
  set_presence(h,-1);WdfDeviceSetFailed(h->device,WdfDeviceFailedNoRestart);return;
 }
 status=SdAcpi(h,"SDCD",&present);set_presence(h,NT_SUCCESS(status)&&present<=1?(LONG)present:-1);
 if(!InterlockedCompareExchange(&h->closing,0,0))WdfTimerStart(timer,WDF_REL_TIMEOUT_IN_MS(100));
}
static NTSTATUS prepare_impl(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 struct sd_host *h=SdHost(device);BOOLEAN memory=FALSE,irq=FALSE;PHYSICAL_ADDRESS address={0};
 if(WdfCmResourceListGetCount(raw)!=WdfCmResourceListGetCount(translated))return STATUS_DEVICE_CONFIGURATION_ERROR;
 for(ULONG i=0;i<WdfCmResourceListGetCount(translated);i++){
  PCM_PARTIAL_RESOURCE_DESCRIPTOR t=WdfCmResourceListGetDescriptor(translated,i),r=WdfCmResourceListGetDescriptor(raw,i);
  if(t->Type==CmResourceTypeMemory){
   if(memory||r->Type!=CmResourceTypeMemory||r->u.Memory.Start.QuadPart!=A04_SD_BASE||r->u.Memory.Length!=0x1000||t->u.Memory.Length!=0x1000||t->u.Memory.Start.QuadPart!=A04_SD_BASE)return STATUS_DEVICE_CONFIGURATION_ERROR;
   address=t->u.Memory.Start;memory=TRUE;
  }else if(t->Type==CmResourceTypeInterrupt){if(irq||r->Type!=CmResourceTypeInterrupt||r->u.Interrupt.Vector!=A04_SD_IRQ)return STATUS_DEVICE_CONFIGURATION_ERROR;irq=TRUE;}
  else if(t->Type==CmResourceTypePort||t->Type==CmResourceTypeDma)return STATUS_DEVICE_CONFIGURATION_ERROR;
 }
 if(!memory||!irq)return STATUS_DEVICE_CONFIGURATION_ERROR;
 h->registers=MmMapIoSpace(address,0x1000,MmNonCached);if(!h->registers)return STATUS_INSUFFICIENT_RESOURCES;
 NTSTATUS status=SdPowerConnect(h);
 if(!NT_SUCCESS(status)){MmUnmapIoSpace((PVOID)h->registers,0x1000);h->registers=NULL;}
 return status;
}
NTSTATUS SdPrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
 SdCheckpoint(L"PrepareHardware",1,STATUS_PENDING,FALSE);
 NTSTATUS status=prepare_impl(device,raw,translated);SdCheckpoint(L"PrepareHardware",2,status,TRUE);return status;
}
NTSTATUS SdRelease(WDFDEVICE device,WDFCMRESLIST resources)
{
 struct sd_host *h=SdHost(device);UNREFERENCED_PARAMETER(resources);
 SdStopAdmission(h);WdfTimerStop(h->timer,TRUE);
 NTSTATUS status=SdConsumerStop(h);if(!NT_SUCCESS(status))return status;
 status=SdPowerDisconnect(h);if(!NT_SUCCESS(status))return status;
 /* D0Exit has joined every accepted IRP/worker before resource release. */
 if(h->registers){MmUnmapIoSpace((PVOID)h->registers,0x1000);h->registers=NULL;}
 RtlSecureZeroMemory(&h->card,sizeof(h->card));return STATUS_SUCCESS;
}
static NTSTATUS d0_impl(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
 struct sd_host *h=SdHost(device);ULONG proof=0,present=0;UNREFERENCED_PARAMETER(previous);
 if(h->surprised||h->supplier_lost)return STATUS_DEVICE_NOT_CONNECTED;
 NTSTATUS status=SdAcpi(h,"SDCF",&proof);if(!NT_SUCCESS(status))return status;
 if(proof!=1)return STATUS_DEVICE_CONFIGURATION_ERROR;
 status=SdAcpi(h,"SDCD",&present);if(!NT_SUCCESS(status)||present>1)return NT_SUCCESS(status)?STATUS_DEVICE_DATA_ERROR:status;
 if(!h->power_valid||h->power_token)return STATUS_INVALID_DEVICE_STATE;
 status=h->power.Acquire(h->power.Header.Context,&h->power_token);if(!NT_SUCCESS(status))return status;
 /* Claim changes no rail/gate. Existing UEFI-ready clocks permit this
  * observation; never revive gated firmware DMA or abort its descriptors. */
 if(READ_REGISTER_ULONG(h->registers+0x09c/4)&1u)return STATUS_NOT_SUPPORTED;
 status=h->power.Enable(h->power.Header.Context,h->power_token);if(!NT_SUCCESS(status))return status;
 RtlSecureZeroMemory(&h->card,sizeof(h->card));h->card_generation=-1;h->active_irp=NULL;
 h->ops.io.context=h;h->ops.io.read32=read32;h->ops.io.read8=read8;h->ops.io.write32=write32;
 h->ops.now_100ns=now_100ns;h->ops.delay_us=delay_us;h->ops.live=live;
 h->quiescing=TRUE;enum a04_sd_result reset=a04_sd_quiesce(&h->ops,now_100ns(h)+2000000u);h->quiescing=FALSE;
 if(reset!=SD_OK)return STATUS_IO_DEVICE_ERROR;
 if(present==0||!h->had_d0)InterlockedExchange(&h->ejected,0);
 if(h->had_d0)ExReInitializeRundownProtection(&h->requests);h->had_d0=TRUE;
 InterlockedIncrement(&h->generation);InterlockedExchange(&h->present,(LONG)present);InterlockedExchange(&h->attention,1);
 InterlockedExchange(&h->absent_seen,present==0);if(present==0)InterlockedExchange(&h->blocked,0);
 status=SdConsumerStart(h);if(!NT_SUCCESS(status))return status;
 WdfTimerStart(h->timer,WDF_REL_TIMEOUT_IN_MS(100));return STATUS_SUCCESS;
}
NTSTATUS SdD0Entry(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
 SdCheckpoint(L"D0Entry",1,STATUS_PENDING,FALSE);
 NTSTATUS status=d0_impl(device,previous);SdCheckpoint(L"D0Entry",2,status,TRUE);return status;
}
NTSTATUS SdD0Exit(WDFDEVICE device,WDF_POWER_DEVICE_STATE target)
{
 struct sd_host *h=SdHost(device);UNREFERENCED_PARAMETER(target);
 SdStopAdmission(h);InterlockedIncrement(&h->generation);WdfTimerStop(h->timer,TRUE);
 NTSTATUS joined=SdConsumerStop(h);if(!NT_SUCCESS(joined))return joined;
 LARGE_INTEGER serial_wait;serial_wait.QuadPart=-10000000;
 if(KeWaitForSingleObject(&h->serial,Executive,KernelMode,FALSE,&serial_wait)!=STATUS_SUCCESS)return STATUS_IO_TIMEOUT;
 /* Join CPU users, wait for card programming, then reset the private PIO
  * host before asking the supplier to restore only SD-owned resources. */
 h->active_irp=NULL;h->quiescing=TRUE;
 enum a04_sd_result result=SD_OK;
 if(h->power_token && !NT_SUCCESS(SdPowerHealth(h)))result=SD_POISONED;
 else if(h->power_token&&!h->surprised&&h->registers){
  if(h->present!=0&&(h->card.poisoned||h->blocked))result=SD_POISONED;
  else if(h->present==1&&h->card.ready)result=a04_sd_barrier(&h->ops,&h->card,now_100ns(h)+2000000u);
  if(result==SD_OK)result=a04_sd_quiesce(&h->ops,now_100ns(h)+2000000u);
 }
 h->quiescing=FALSE;
 if(h->card.poisoned&&h->present!=0)InterlockedExchange(&h->blocked,1);
 RtlSecureZeroMemory(&h->card,sizeof(h->card));h->card_generation=-1;
 NTSTATUS status=result==SD_OK?STATUS_SUCCESS:STATUS_IO_DEVICE_ERROR;
 if(h->surprised&&h->power_token)status=STATUS_DEVICE_NOT_CONNECTED;
 if(NT_SUCCESS(status))status=SdPowerRelease(h);
 KeReleaseMutex(&h->serial,FALSE);return status;
}
VOID SdSurprise(WDFDEVICE device)
{
 struct sd_host *h=SdHost(device);InterlockedExchange(&h->surprised,1);SdStopAdmission(h);InterlockedExchange(&h->present,0);InterlockedIncrement(&h->generation);
 /* No MMIO, reset, unmap or early IRP completion on surprise notification. */
}
VOID SdExecute(struct sd_work *w)
{
 struct sd_host *h=w->host;ULONG_PTR information=0;NTSTATUS status;LARGE_INTEGER timeout;
 uint64_t now=KeQueryInterruptTime();
 if(w->irp->Cancel){SdFinish(w,STATUS_CANCELLED,0);return;}
 if(h->closing||w->generation!=h->generation){SdFinish(w,STATUS_DEVICE_NOT_CONNECTED,0);return;}
 if(now>=w->deadline){SdFinish(w,STATUS_IO_TIMEOUT,0);return;}
 timeout.QuadPart=-(LONGLONG)(w->deadline-now);
 status=KeWaitForSingleObject(&h->serial,Executive,KernelMode,FALSE,&timeout);
 if(status!=STATUS_SUCCESS){SdFinish(w,STATUS_IO_TIMEOUT,0);return;}
 if(h->closing||w->generation!=h->generation)status=STATUS_DEVICE_NOT_CONNECTED;
 else if(w->irp->Cancel)status=STATUS_CANCELLED;
 else if(KeQueryInterruptTime()>=w->deadline)status=STATUS_IO_TIMEOUT;
 else if(!NT_SUCCESS(status=SdPowerHealth(h)))InterlockedExchange(&h->blocked,1);
 else {h->run_generation=w->generation;h->active_irp=w->irp;status=SdProcess(w,&information);
  if(h->card.poisoned&&(w->generation==h->generation||h->closing))InterlockedExchange(&h->blocked,1);h->active_irp=NULL;
  if(w->irp->Cancel){status=STATUS_CANCELLED;information=0;}
 }
 KeReleaseMutex(&h->serial,FALSE);
 SdFinish(w,status,information);
}
/* Standard classpnp management without card/MMIO effects. PDO identity is
 * borrowed from the live PnP stack; no fabricated card identifier is returned. */
static int management(WDFDEVICE child,PIRP irp,NTSTATUS *status)
{
 PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(irp);if(stack->MajorFunction!=IRP_MJ_SCSI)return 0;
 PSCSI_REQUEST_BLOCK s=stack->Parameters.Scsi.Srb;struct sd_host *h=SdChild(child)->host;
 if(!s||s->Length<sizeof(*s))return 0;
 if(s->Function==SRB_FUNCTION_CLAIM_DEVICE){
  if(InterlockedCompareExchange(&h->claimed,1,0)){*status=STATUS_DEVICE_BUSY;s->SrbStatus=SRB_STATUS_BUSY;}
  else {s->DataBuffer=WdfDeviceWdmGetDeviceObject(child);s->SrbStatus=SRB_STATUS_SUCCESS;*status=STATUS_SUCCESS;}
  s->DataTransferLength=0;return 1;
 }
 if(s->Function==SRB_FUNCTION_RELEASE_DEVICE){
  LONG claimed=InterlockedExchange(&h->claimed,0);s->SrbStatus=claimed?SRB_STATUS_SUCCESS:SRB_STATUS_INVALID_REQUEST;s->DataTransferLength=0;
  *status=claimed?STATUS_SUCCESS:STATUS_INVALID_DEVICE_STATE;return 1;
 }
 if(s->Function!=SRB_FUNCTION_PNP||s->Length<sizeof(SCSI_PNP_REQUEST_BLOCK))return 0;
 PSCSI_PNP_REQUEST_BLOCK p=(PSCSI_PNP_REQUEST_BLOCK)s;*status=STATUS_NOT_SUPPORTED;p->SrbStatus=SRB_STATUS_INVALID_REQUEST;
 if(p->SrbPnPFlags&SRB_PNP_FLAGS_ADAPTER_REQUEST)return 1;
 if(p->PnPAction==StorQueryCapabilities&&p->DataBuffer){
  if(p->DataTransferLength>=sizeof(STOR_DEVICE_CAPABILITIES_EX)){
   PSTOR_DEVICE_CAPABILITIES_EX caps=p->DataBuffer;RtlZeroMemory(caps,sizeof(*caps));caps->Version=1;caps->Size=sizeof(*caps);caps->Removable=1;caps->SurpriseRemovalOK=1;p->DataTransferLength=sizeof(*caps);
  }else if(p->DataTransferLength>=sizeof(STOR_DEVICE_CAPABILITIES)){
   PSTOR_DEVICE_CAPABILITIES caps=p->DataBuffer;RtlZeroMemory(caps,sizeof(*caps));caps->Version=1;caps->Removable=1;caps->SurpriseRemovalOK=1;p->DataTransferLength=sizeof(*caps);
  }else {*status=STATUS_BUFFER_TOO_SMALL;return 1;}
 }else if(p->PnPAction==StorStartDevice){
  if(h->closing)return 1;InterlockedExchange(&h->child_online,1);p->DataTransferLength=0;
 }else if(p->PnPAction==StorStopDevice||p->PnPAction==StorRemoveDevice||p->PnPAction==StorSurpriseRemoval){
  InterlockedExchange(&h->child_online,0);if(h->inflight)InterlockedExchange(&h->blocked,1);
  InterlockedIncrement(&h->generation);InterlockedExchange(&h->attention,1);p->DataTransferLength=0;
 }else return 1;
 p->SrbStatus=SRB_STATUS_SUCCESS;*status=STATUS_SUCCESS;return 1;
}
NTSTATUS SdScsi(WDFDEVICE child,PIRP irp)
{
 struct sd_host *h=SdChild(child)->host;NTSTATUS status=STATUS_DEVICE_NOT_READY;
 if(management(child,irp,&status)){irp->IoStatus.Status=status;irp->IoStatus.Information=0;IoCompleteRequest(irp,IO_NO_INCREMENT);return status;}
 return SdSubmit(child,irp);
}
NTSTATUS SdCreateChild(struct sd_host *h,ULONG *stage)
{
 *stage=20;PWDFDEVICE_INIT init=WdfPdoInitAllocate(h->device);WDFDEVICE child;WDF_OBJECT_ATTRIBUTES attributes;NTSTATUS status;
 if(!init)return STATUS_INSUFFICIENT_RESOURCES;
 DECLARE_CONST_UNICODE_STRING(id,L"MT8167\\SDMEM");DECLARE_CONST_UNICODE_STRING(compatible,L"GenDisk");
 DECLARE_CONST_UNICODE_STRING(instance,L"MSDC1-Socket");DECLARE_CONST_UNICODE_STRING(text,L"MediaTek MT8167 MSDC1 microSD Card");
 /* Preserve SetPdo's FILE_AUTOGENERATED_DEVICE_NAME; FALSE erased it and
  * InitializePdo returned STATUS_OBJECT_NAME_INVALID before Prepare/D0. */
 WdfDeviceInitSetDeviceType(init,FILE_DEVICE_DISK);WdfDeviceInitSetCharacteristics(init,FILE_REMOVABLE_MEDIA,TRUE);WdfDeviceInitSetIoType(init,WdfDeviceIoDirect);
 *stage=21;status=WdfPdoInitAssignDeviceID(init,&id);if(!NT_SUCCESS(status))goto fail;
 *stage=22;status=WdfPdoInitAddHardwareID(init,&id);if(!NT_SUCCESS(status))goto fail;
 *stage=23;status=WdfPdoInitAddCompatibleID(init,&compatible);if(!NT_SUCCESS(status))goto fail;
 *stage=24;status=WdfPdoInitAssignInstanceID(init,&instance);if(!NT_SUCCESS(status))goto fail;
 *stage=25;status=WdfPdoInitAddDeviceText(init,&text,&text,0x409);if(!NT_SUCCESS(status))goto fail;WdfPdoInitSetDefaultLocale(init,0x409);
 *stage=26;status=WdfDeviceInitAssignWdmIrpPreprocessCallback(init,SdScsi,IRP_MJ_SCSI,NULL,0);if(!NT_SUCCESS(status))goto fail;
 *stage=27;status=WdfDeviceInitAssignWdmIrpPreprocessCallback(init,SdScsi,IRP_MJ_FLUSH_BUFFERS,NULL,0);if(!NT_SUCCESS(status))goto fail;
 *stage=28;WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,SD_CHILD);attributes.ExecutionLevel=WdfExecutionLevelPassive;status=WdfDeviceCreate(&init,&attributes,&child);if(!NT_SUCCESS(status))goto fail;
 SdChild(child)->host=h;h->child=child;
 WDF_DEVICE_PNP_CAPABILITIES caps;WDF_DEVICE_PNP_CAPABILITIES_INIT(&caps);caps.Removable=WdfTrue;caps.EjectSupported=WdfFalse;caps.SurpriseRemovalOK=WdfTrue;caps.UniqueID=WdfFalse;caps.Address=0;caps.UINumber=0;
 WdfDeviceSetPnpCapabilities(child,&caps);
 WdfDeviceSetSpecialFileSupport(child,WdfSpecialFilePaging,FALSE);WdfDeviceSetSpecialFileSupport(child,WdfSpecialFileHibernation,FALSE);WdfDeviceSetSpecialFileSupport(child,WdfSpecialFileDump,FALSE);
 WDF_IO_QUEUE_CONFIG queue;WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue,WdfIoQueueDispatchParallel);queue.EvtIoDeviceControl=SdControl;
 *stage=30;status=WdfIoQueueCreate(child,&queue,WDF_NO_OBJECT_ATTRIBUTES,NULL);if(!NT_SUCCESS(status)){WdfObjectDelete(child);h->child=NULL;return status;}
 *stage=31;status=WdfFdoAddStaticChild(h->device,child);if(!NT_SUCCESS(status)){WdfObjectDelete(child);h->child=NULL;}return status;
fail:if(init)WdfDeviceInitFree(init);return status;
}
static NTSTATUS add_impl(WDFDRIVER driver,PWDFDEVICE_INIT init,ULONG *stage)
{
 WDFDEVICE device;WDF_OBJECT_ATTRIBUTES attributes;WDF_PNPPOWER_EVENT_CALLBACKS pnp;NTSTATUS status;UNREFERENCED_PARAMETER(driver);
 UCHAR pnpMinor[]={IRP_MN_QUERY_DEVICE_RELATIONS,IRP_MN_DEVICE_USAGE_NOTIFICATION};
 *stage=1;status=WdfDeviceInitAssignWdmIrpPreprocessCallback(init,SdPowerPnp,IRP_MJ_PNP,pnpMinor,sizeof(pnpMinor));if(!NT_SUCCESS(status))return status;
 WdfDeviceInitSetDeviceType(init,FILE_DEVICE_CONTROLLER);WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
 pnp.EvtDevicePrepareHardware=SdPrepare;pnp.EvtDeviceReleaseHardware=SdRelease;pnp.EvtDeviceD0Entry=SdD0Entry;pnp.EvtDeviceD0Exit=SdD0Exit;pnp.EvtDeviceSurpriseRemoval=SdSurprise;
 WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,SD_HOST);attributes.ExecutionLevel=WdfExecutionLevelPassive;attributes.SynchronizationScope=WdfSynchronizationScopeNone;attributes.EvtDestroyCallback=SdConsumerDestroy;
 *stage=2;status=WdfDeviceCreate(&init,&attributes,&device);if(!NT_SUCCESS(status))return status;
 PNP_BUS_INFORMATION bus;RtlZeroMemory(&bus,sizeof(bus));bus.BusTypeGuid=GUID_BUS_TYPE_SD;bus.LegacyBusType=PNPBus;bus.BusNumber=1;
 WdfDeviceSetBusInformationForChildren(device,&bus);
 struct sd_host *h=SdHost(device);h->device=device;h->closing=1;h->child_online=1;h->card_generation=-1;ExInitializeFastMutex(&h->power_relations_lock);ExInitializeRundownProtection(&h->requests);KeInitializeMutex(&h->serial,0);
 *stage=4;status=SdConsumerInitialize(h);if(!NT_SUCCESS(status))return status;
 WDF_TIMER_CONFIG timer;WDF_TIMER_CONFIG_INIT(&timer,SdTimer);timer.AutomaticSerialization=FALSE;WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=device;attributes.ExecutionLevel=WdfExecutionLevelPassive;
 *stage=3;status=WdfTimerCreate(&timer,&attributes,&h->timer);if(!NT_SUCCESS(status))return status;
 return SdCreateChild(h,stage);
}
NTSTATUS SdAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 ULONG stage=0;SdCheckpoint(L"AddDevice",0x100,STATUS_PENDING,FALSE);
 NTSTATUS status=add_impl(driver,init,&stage);SdCheckpoint(L"AddDevice",stage,status,TRUE);return status;
}
EVT_WDF_DRIVER_UNLOAD SdUnload;
VOID SdUnload(WDFDRIVER driver){UNREFERENCED_PARAMETER(driver);SdCheckpointClose();}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING registry)
{
 WDF_DRIVER_CONFIG config;NTSTATUS status;SdCheckpointInitialize(registry);
 SdCheckpoint(L"AddDevice",0,STATUS_PENDING,FALSE);SdCheckpoint(L"PrepareHardware",0,STATUS_PENDING,FALSE);SdCheckpoint(L"D0Entry",0,STATUS_PENDING,FALSE);
 SdCheckpoint(L"DriverEntry",1,STATUS_PENDING,FALSE);WDF_DRIVER_CONFIG_INIT(&config,SdAdd);config.EvtDriverUnload=SdUnload;
 status=WdfDriverCreate(object,registry,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
 SdCheckpoint(L"DriverEntry",2,status,TRUE);if(!NT_SUCCESS(status))SdCheckpointClose();return status;
}
