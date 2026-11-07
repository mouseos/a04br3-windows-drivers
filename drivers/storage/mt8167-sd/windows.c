/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * KMDF physical host + persistent removable GenDisk PDO. Standard disk.sys
 * owns the disk class/partition/volume stack. No virtual Storport worker whose
 * extension or DMA common buffer can disappear underneath an outstanding SRB.
 */
#include <initguid.h>
#include "windows.h"
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
 NTSTATUS status=SdAcpi(h,"SDCD",&present);set_presence(h,NT_SUCCESS(status)&&present<=1?(LONG)present:-1);
 if(!InterlockedCompareExchange(&h->closing,0,0))WdfTimerStart(timer,WDF_REL_TIMEOUT_IN_MS(100));
}
NTSTATUS SdPrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated)
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
 return STATUS_SUCCESS;
}
NTSTATUS SdRelease(WDFDEVICE device,WDFCMRESLIST resources)
{
 struct sd_host *h=SdHost(device);UNREFERENCED_PARAMETER(resources);
 InterlockedExchange(&h->closing,1);WdfTimerStop(h->timer,TRUE);
 /* D0Exit has joined every accepted IRP/worker before resource release. */
 if(h->registers){MmUnmapIoSpace((PVOID)h->registers,0x1000);h->registers=NULL;}
 RtlSecureZeroMemory(&h->card,sizeof(h->card));return STATUS_SUCCESS;
}
NTSTATUS SdD0Entry(WDFDEVICE device,WDF_POWER_DEVICE_STATE previous)
{
 struct sd_host *h=SdHost(device);ULONG proof=0,present=0;UNREFERENCED_PARAMETER(previous);
 NTSTATUS status=SdAcpi(h,"SDCF",&proof);if(!NT_SUCCESS(status))return status;
 if(proof!=1)return STATUS_DEVICE_CONFIGURATION_ERROR;
 status=SdAcpi(h,"SDCD",&present);if(!NT_SUCCESS(status)||present>1)return NT_SUCCESS(status)?STATUS_DEVICE_DATA_ERROR:status;
 RtlSecureZeroMemory(&h->card,sizeof(h->card));h->card_generation=-1;h->active_irp=NULL;
 h->ops.io.context=h;h->ops.io.read32=read32;h->ops.io.read8=read8;h->ops.io.write32=write32;
 h->ops.now_100ns=now_100ns;h->ops.delay_us=delay_us;h->ops.live=live;
 h->quiescing=TRUE;enum a04_sd_result reset=a04_sd_quiesce(&h->ops,now_100ns(h)+2000000u);h->quiescing=FALSE;
 if(reset!=SD_OK)return STATUS_IO_DEVICE_ERROR;
 if(present==0||!h->had_d0)InterlockedExchange(&h->ejected,0);
 if(h->had_d0)ExReInitializeRundownProtection(&h->requests);h->had_d0=TRUE;
 InterlockedIncrement(&h->generation);InterlockedExchange(&h->present,(LONG)present);InterlockedExchange(&h->attention,1);
 InterlockedExchange(&h->absent_seen,present==0);if(present==0)InterlockedExchange(&h->blocked,0);
 InterlockedExchange(&h->closing,0);
 WdfTimerStart(h->timer,WDF_REL_TIMEOUT_IN_MS(100));return STATUS_SUCCESS;
}
NTSTATUS SdD0Exit(WDFDEVICE device,WDF_POWER_DEVICE_STATE target)
{
 struct sd_host *h=SdHost(device);UNREFERENCED_PARAMETER(target);
 InterlockedExchange(&h->closing,1);InterlockedIncrement(&h->generation);WdfTimerStop(h->timer,TRUE);WdfTimerStop(h->timer,TRUE);
 ExWaitForRundownProtectionRelease(&h->requests);
 KeWaitForSingleObject(&h->serial,Executive,KernelMode,FALSE,NULL);
 /* PIO only: no descriptor/client buffer is reachable by hardware. Check and
  * reset the private host after all CPU users exited; never cut shared rails. */
 h->active_irp=NULL;h->quiescing=TRUE;
 enum a04_sd_result result=SD_OK;
 if(!h->surprised&&h->registers)result=a04_sd_quiesce(&h->ops,now_100ns(h)+2000000u);h->quiescing=FALSE;
 if(h->card.poisoned&&h->present!=0)InterlockedExchange(&h->blocked,1);
 RtlSecureZeroMemory(&h->card,sizeof(h->card));h->card_generation=-1;
 KeReleaseMutex(&h->serial,FALSE);return result==SD_OK?STATUS_SUCCESS:STATUS_IO_DEVICE_ERROR;
}
VOID SdSurprise(WDFDEVICE device)
{
 struct sd_host *h=SdHost(device);InterlockedExchange(&h->surprised,1);InterlockedExchange(&h->closing,1);InterlockedExchange(&h->present,0);InterlockedIncrement(&h->generation);
 /* No MMIO, reset, unmap or early IRP completion on surprise notification. */
}
static VOID work(PDEVICE_OBJECT device,PVOID context)
{
 UNREFERENCED_PARAMETER(device);
 struct sd_work *w=context;struct sd_host *h=w->host;ULONG_PTR information=0;NTSTATUS status;
 KeWaitForSingleObject(&h->serial,Executive,KernelMode,FALSE,NULL);
 if(h->closing||w->generation!=h->generation)status=STATUS_DEVICE_NOT_CONNECTED;
 else if(w->irp->Cancel)status=STATUS_CANCELLED;
 else if(KeQueryInterruptTime()>=w->deadline)status=STATUS_IO_TIMEOUT;
 else {h->run_generation=w->generation;h->active_irp=w->irp;status=SdProcess(w,&information);
  if(h->card.poisoned&&(w->generation==h->generation||h->closing))InterlockedExchange(&h->blocked,1);h->active_irp=NULL;}
 KeReleaseMutex(&h->serial,FALSE);
 if(!NT_SUCCESS(status)){
  PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(w->irp);
  if(stack->MajorFunction==IRP_MJ_SCSI&&stack->Parameters.Scsi.Srb&&stack->Parameters.Scsi.Srb->Length>=sizeof(SCSI_REQUEST_BLOCK)){
   PSCSI_REQUEST_BLOCK srb=stack->Parameters.Scsi.Srb;srb->DataTransferLength=0;
   if(srb->SrbStatus==SRB_STATUS_PENDING||srb->SrbStatus==SRB_STATUS_SUCCESS)srb->SrbStatus=status==STATUS_CANCELLED?SRB_STATUS_ABORTED:status==STATUS_IO_TIMEOUT?SRB_STATUS_TIMEOUT:SRB_STATUS_NO_DEVICE;
  }
 }
 /* We alone complete this IRP. Cancel/stop cannot detach an SRB while the CPU
  * still accesses its MDL or the caller's sense/data buffers. */
 w->irp->IoStatus.Status=status;w->irp->IoStatus.Information=information;
 /* CPU/MDL/MMIO access is finished. Release hardware rundown before invoking
  * an upper completion which may itself request a power transition. Object
  * references and IoWorkItem still pin our context/module until return. */
 ExReleaseRundownProtection(&h->requests);IoCompleteRequest(w->irp,IO_DISK_INCREMENT);
 InterlockedDecrement(&h->inflight);
 IoFreeWorkItem(w->item);WdfObjectDereference(w->child);WdfObjectDereference(h->device);ExFreePoolWithTag(w,SD_TAG);
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
 if(!h->closing&&ExAcquireRundownProtection(&h->requests)){
  if(InterlockedIncrement(&h->inflight)<=8){
   struct sd_work *w=ExAllocatePoolWithTag(NonPagedPool,sizeof(*w),SD_TAG);
   if(w)w->item=IoAllocateWorkItem(WdfDeviceWdmGetDeviceObject(h->device));
   if(w&&w->item){w->host=h;w->child=child;w->irp=irp;w->generation=InterlockedCompareExchange(&h->generation,0,0);
    ULONG seconds=5;PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(irp);
    if(stack->MajorFunction==IRP_MJ_SCSI&&stack->Parameters.Scsi.Srb&&stack->Parameters.Scsi.Srb->Length>=sizeof(SCSI_REQUEST_BLOCK)&&stack->Parameters.Scsi.Srb->TimeOutValue)seconds=stack->Parameters.Scsi.Srb->TimeOutValue;
    if(seconds>10)seconds=10;w->deadline=KeQueryInterruptTime()+(uint64_t)seconds*10000000u;
    WdfObjectReference(child);WdfObjectReference(h->device);IoMarkIrpPending(irp);IoQueueWorkItem(w->item,work,DelayedWorkQueue,w);return STATUS_PENDING;}
   if(w)ExFreePoolWithTag(w,SD_TAG);status=STATUS_INSUFFICIENT_RESOURCES;
  }else status=STATUS_DEVICE_BUSY;
  InterlockedDecrement(&h->inflight);ExReleaseRundownProtection(&h->requests);
 }
 PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(irp);
 if(stack->MajorFunction==IRP_MJ_SCSI&&stack->Parameters.Scsi.Srb&&stack->Parameters.Scsi.Srb->Length>=sizeof(SCSI_REQUEST_BLOCK)){
  PSCSI_REQUEST_BLOCK s=stack->Parameters.Scsi.Srb;s->DataTransferLength=0;if(s->Function!=SRB_FUNCTION_POWER)s->ScsiStatus=0;
  s->SrbStatus=status==STATUS_DEVICE_BUSY||status==STATUS_INSUFFICIENT_RESOURCES?SRB_STATUS_BUSY:SRB_STATUS_NO_DEVICE;
 }
 irp->IoStatus.Status=status;irp->IoStatus.Information=0;IoCompleteRequest(irp,IO_NO_INCREMENT);return status;
}
NTSTATUS SdCreateChild(struct sd_host *h)
{
 PWDFDEVICE_INIT init=WdfPdoInitAllocate(h->device);WDFDEVICE child;WDF_OBJECT_ATTRIBUTES attributes;NTSTATUS status;
 if(!init)return STATUS_INSUFFICIENT_RESOURCES;
 DECLARE_CONST_UNICODE_STRING(id,L"MT8167\\SDMEM");DECLARE_CONST_UNICODE_STRING(compatible,L"GenDisk");
 DECLARE_CONST_UNICODE_STRING(instance,L"MSDC1-Socket");DECLARE_CONST_UNICODE_STRING(text,L"MediaTek MT8167 MSDC1 microSD Card");
 WdfDeviceInitSetDeviceType(init,FILE_DEVICE_DISK);WdfDeviceInitSetCharacteristics(init,FILE_REMOVABLE_MEDIA,FALSE);WdfDeviceInitSetIoType(init,WdfDeviceIoDirect);
 status=WdfPdoInitAssignDeviceID(init,&id);if(!NT_SUCCESS(status))goto fail;
 status=WdfPdoInitAddHardwareID(init,&id);if(!NT_SUCCESS(status))goto fail;
 status=WdfPdoInitAddCompatibleID(init,&compatible);if(!NT_SUCCESS(status))goto fail;
 status=WdfPdoInitAssignInstanceID(init,&instance);if(!NT_SUCCESS(status))goto fail;
 status=WdfPdoInitAddDeviceText(init,&text,&text,0x409);if(!NT_SUCCESS(status))goto fail;WdfPdoInitSetDefaultLocale(init,0x409);
 status=WdfDeviceInitAssignWdmIrpPreprocessCallback(init,SdScsi,IRP_MJ_SCSI,NULL,0);if(!NT_SUCCESS(status))goto fail;
 status=WdfDeviceInitAssignWdmIrpPreprocessCallback(init,SdScsi,IRP_MJ_FLUSH_BUFFERS,NULL,0);if(!NT_SUCCESS(status))goto fail;
 WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,SD_CHILD);attributes.ExecutionLevel=WdfExecutionLevelPassive;status=WdfDeviceCreate(&init,&attributes,&child);if(!NT_SUCCESS(status))goto fail;
 SdChild(child)->host=h;h->child=child;
 WDF_DEVICE_PNP_CAPABILITIES caps;WDF_DEVICE_PNP_CAPABILITIES_INIT(&caps);caps.Removable=WdfTrue;caps.EjectSupported=WdfFalse;caps.SurpriseRemovalOK=WdfTrue;caps.UniqueID=WdfFalse;caps.Address=0;caps.UINumber=0;
 WdfDeviceSetPnpCapabilities(child,&caps);
 WdfDeviceSetSpecialFileSupport(child,WdfSpecialFilePaging,FALSE);WdfDeviceSetSpecialFileSupport(child,WdfSpecialFileHibernation,FALSE);WdfDeviceSetSpecialFileSupport(child,WdfSpecialFileDump,FALSE);
 WDF_IO_QUEUE_CONFIG queue;WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue,WdfIoQueueDispatchParallel);queue.EvtIoDeviceControl=SdControl;
 status=WdfIoQueueCreate(child,&queue,WDF_NO_OBJECT_ATTRIBUTES,NULL);if(!NT_SUCCESS(status)){WdfObjectDelete(child);h->child=NULL;return status;}
 status=WdfFdoAddStaticChild(h->device,child);if(!NT_SUCCESS(status)){WdfObjectDelete(child);h->child=NULL;}return status;
fail:if(init)WdfDeviceInitFree(init);return status;
}
NTSTATUS SdAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
 WDFDEVICE device;WDF_OBJECT_ATTRIBUTES attributes;WDF_PNPPOWER_EVENT_CALLBACKS pnp;NTSTATUS status;UNREFERENCED_PARAMETER(driver);
 WdfDeviceInitSetDeviceType(init,FILE_DEVICE_CONTROLLER);WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
 pnp.EvtDevicePrepareHardware=SdPrepare;pnp.EvtDeviceReleaseHardware=SdRelease;pnp.EvtDeviceD0Entry=SdD0Entry;pnp.EvtDeviceD0Exit=SdD0Exit;pnp.EvtDeviceSurpriseRemoval=SdSurprise;
 WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,SD_HOST);attributes.ExecutionLevel=WdfExecutionLevelPassive;attributes.SynchronizationScope=WdfSynchronizationScopeNone;
 status=WdfDeviceCreate(&init,&attributes,&device);if(!NT_SUCCESS(status))return status;
 PNP_BUS_INFORMATION bus;RtlZeroMemory(&bus,sizeof(bus));bus.BusTypeGuid=GUID_BUS_TYPE_SD;bus.LegacyBusType=PNPBus;bus.BusNumber=1;
 WdfDeviceSetBusInformationForChildren(device,&bus);
 struct sd_host *h=SdHost(device);h->device=device;h->closing=1;h->child_online=1;h->card_generation=-1;ExInitializeRundownProtection(&h->requests);KeInitializeMutex(&h->serial,0);
 WDF_TIMER_CONFIG timer;WDF_TIMER_CONFIG_INIT(&timer,SdTimer);timer.AutomaticSerialization=FALSE;WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=device;attributes.ExecutionLevel=WdfExecutionLevelPassive;
 status=WdfTimerCreate(&timer,&attributes,&h->timer);if(!NT_SUCCESS(status))return status;
 return SdCreateChild(h);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT object,PUNICODE_STRING registry)
{WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,SdAdd);return WdfDriverCreate(object,registry,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);}
