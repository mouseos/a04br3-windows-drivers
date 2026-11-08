/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Shares Mt8167PnpBackend, its single DL1, mutex, MDL/SG and rundown. */
#include "wavert_miniport.h"
static NTSTATUS HpQueryComplete(PDEVICE_OBJECT,PIRP,PVOID context)
{KeSetEvent((PKEVENT)context,IO_NO_INCREMENT,FALSE);return STATUS_MORE_PROCESSING_REQUIRED;}
static NTSTATUS HpQueryParent(PDEVICE_OBJECT target,A04_HEADPHONE_INTERFACE *out)
{
 KEVENT event;PIRP irp;PIO_STACK_LOCATION stack;NTSTATUS status;
 RtlZeroMemory(out,sizeof(*out));KeInitializeEvent(&event,NotificationEvent,FALSE);
 irp=IoAllocateIrp(target->StackSize,FALSE);if(!irp)return STATUS_INSUFFICIENT_RESOURCES;
 irp->IoStatus.Status=STATUS_NOT_SUPPORTED;irp->IoStatus.Information=0;
 stack=IoGetNextIrpStackLocation(irp);RtlZeroMemory(stack,sizeof(*stack));
 stack->MajorFunction=IRP_MJ_PNP;stack->MinorFunction=IRP_MN_QUERY_INTERFACE;
 stack->Parameters.QueryInterface.InterfaceType=&A04HeadphoneParentGuid;
 stack->Parameters.QueryInterface.Size=sizeof(*out);stack->Parameters.QueryInterface.Version=A04_HP_PARENT_VERSION;
 stack->Parameters.QueryInterface.Interface=(PINTERFACE)out;
 IoSetCompletionRoutine(irp,HpQueryComplete,&event,TRUE,TRUE,TRUE);
 status=IoCallDriver(target,irp);if(status==STATUS_PENDING)KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);
 status=irp->IoStatus.Status;IoFreeIrp(irp);if(!NT_SUCCESS(status))RtlZeroMemory(out,sizeof(*out));return status;
}
static NTSTATUS HpQualified(void*p)
{
 Mt8167PnpBackend*o=(Mt8167PnpBackend*)p;ULONG flags=0;
 OwnerLock(o);
 if(!o->online||o->enginePoison||!o->hpParent.Header.Context){OwnerUnlock(o);return STATUS_DEVICE_NOT_READY;}
 /* Transport existence is not calibration/latency success. Property returns
  * actual jack status; creating a PCM stream has the stronger admission below. */
 NTSTATUS status=o->hpParent.Readiness(o->hpParent.Header.Context,&flags);OwnerUnlock(o);return status;
}
static NTSTATUS HpReadJack(void*p,struct mt8167_accdet_socket*out)
{Mt8167PnpBackend*o=(Mt8167PnpBackend*)p;OwnerLock(o);NTSTATUS status=o->online?o->hpParent.ReadJack(o->hpParent.Header.Context,out):STATUS_DEVICE_NOT_READY;OwnerUnlock(o);return status;}
static NTSTATUS HpPrepareStream(void*p,KSRTAUDIO_HWLATENCY *latency)
{
 Mt8167PnpBackend*o=(Mt8167PnpBackend*)p;ULONG flags=0;NTSTATUS status;
 if(!latency)return STATUS_INVALID_PARAMETER;OwnerLock(o);
 status=o->online&&!o->enginePoison?o->hpParent.Readiness(o->hpParent.Header.Context,&flags):STATUS_DEVICE_NOT_READY;
 if(NT_SUCCESS(status)&&(flags&(A04_HP_READY_MEASURED|A04_HP_READY_SENSE))!=(A04_HP_READY_MEASURED|A04_HP_READY_SENSE))status=STATUS_DEVICE_NOT_READY;
 if(NT_SUCCESS(status))status=o->hpParent.Latency(o->hpParent.Header.Context,&latency->FifoSize,&latency->ChipsetDelay,&latency->CodecDelay);
 OwnerUnlock(o);return status;
}
static NTSTATUS HpCheckout(void*p,PPORTWAVERTSTREAM port,ULONG bytes,PMDL*mdl,PULONG actual)
{
 Mt8167PnpBackend*o=(Mt8167PnpBackend*)p;KSRTAUDIO_HWLATENCY latency;NTSTATUS status=HpPrepareStream(o,&latency);
 if(!NT_SUCCESS(status))return status;
 return OwnerCheckoutRoute(o,port,bytes,mdl,actual,TRUE);
}
static NTSTATUS HpStart(void*p,BOOLEAN*exposed)
{
 Mt8167PnpBackend*o=(Mt8167PnpBackend*)p;if(!exposed)return STATUS_INVALID_PARAMETER;*exposed=FALSE;
 OwnerLock(o);NTSTATUS s=o->hpSelected?OwnerStartLocked(o,exposed):STATUS_DEVICE_NOT_READY;OwnerUnlock(o);return s;
}
static NTSTATUS HpStop(void*p)
{Mt8167PnpBackend*o=(Mt8167PnpBackend*)p;OwnerLock(o);NTSTATUS s=o->hpSelected?OwnerStopLocked(o):STATUS_DEVICE_NOT_READY;OwnerUnlock(o);return s;}
static NTSTATUS HpWaveFactory(void*p,PMINIPORTWAVERT*out)
{Mt8167PnpBackend*o=(Mt8167PnpBackend*)p;return Mt8167NewMiniportWithBackend(out,&o->hpProvider.stream);}
NTSTATUS Mt8167PnpBackendHeadphone(Mt8167PnpBackend*o,const Mt8167HeadphoneProvider **out)
{
 if(!o||!out||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;*out=NULL;OwnerLock(o);
 NTSTATUS s=STATUS_SUCCESS;
 if(!o->hpParent.Header.Context){
  s=HpQueryParent(o->pdo,&o->hpParent);
  if(NT_SUCCESS(s)&&
   (o->hpParent.Header.Size!=sizeof(o->hpParent)||o->hpParent.Header.Version!=A04_HP_PARENT_VERSION||o->hpParent.ParentBuild!=1105||
    !o->hpParent.Header.Context||!o->hpParent.Header.InterfaceReference||!o->hpParent.Header.InterfaceDereference||
    !o->hpParent.BeginAdc||!o->hpParent.ReadAdc||!o->hpParent.EndAdc||!o->hpParent.Readiness||!o->hpParent.ReadJack||
    !o->hpParent.Start||!o->hpParent.MuteRetained||!o->hpParent.ResumeRetained||!o->hpParent.Stop||!o->hpParent.Latency))s=STATUS_DEVICE_CONFIGURATION_ERROR;
  if(!NT_SUCCESS(s)){
   if(o->hpParent.Header.InterfaceDereference)o->hpParent.Header.InterfaceDereference(o->hpParent.Header.Context);
   RtlZeroMemory(&o->hpParent,sizeof(o->hpParent));
  }else{
   o->hpProvider.stream=o->iface;o->hpProvider.stream.start=HpStart;o->hpProvider.stream.stop=HpStop;
   o->hpProvider.stream.checkout=HpCheckout;o->hpProvider.stream.prepareRenderStream=HpPrepareStream;
   o->hpProvider.stream.renderLatencyValid=FALSE;
   o->hpProvider.qualified=HpQualified;o->hpProvider.readJack=HpReadJack;o->hpProvider.newWaveMiniport=HpWaveFactory;
  }
 }
 if(NT_SUCCESS(s))*out=&o->hpProvider;OwnerUnlock(o);return s;
}
