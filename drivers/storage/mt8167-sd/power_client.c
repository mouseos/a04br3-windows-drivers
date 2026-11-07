/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows.h"
static void lost(WDFIOTARGET target)
{
 struct sd_host*h=SdHost(WdfIoTargetGetDevice(target));
 InterlockedExchange(&h->supplier_lost,1);SdStopAdmission(h);InterlockedExchange(&h->blocked,1);
 InterlockedExchange(&h->present,-1);InterlockedIncrement(&h->generation);
 WdfDeviceSetFailed(h->device,WdfDeviceFailedNoRestart);
}
static NTSTATUS query_remove(WDFIOTARGET target)
{
 struct sd_host*h=SdHost(WdfIoTargetGetDevice(target));
 /* The SD host has not yet drained/quiesced. Never consent to cutting its
  * supplier out from under an active card transfer. */
 if(h->power_token)return STATUS_DEVICE_BUSY;
 lost(target);WdfIoTargetCloseForQueryRemove(target);return STATUS_SUCCESS;
}
static void remove_complete(WDFIOTARGET target)
{lost(target);WdfIoTargetClose(target);}
static void remove_canceled(WDFIOTARGET target)
{
 /* Already failed without restart; cancellation does not authorize a fresh
  * lease or return stale mappings/queries to service. */
 lost(target);WdfIoTargetClose(target);
}
NTSTATUS SdPowerConnect(struct sd_host*h)
{
 PWSTR names=NULL;UNICODE_STRING name;WDF_IO_TARGET_OPEN_PARAMS open;WDF_OBJECT_ATTRIBUTES a;NTSTATUS s;
 s=IoGetDeviceInterfaces(&A04SdPowerInterfaceGuid,NULL,0,&names);if(!NT_SUCCESS(s))return s;
 if(!names[0]){ExFreePool(names);return STATUS_DEVICE_NOT_READY;}
 RtlInitUnicodeString(&name,names);
 if(names[name.Length/sizeof(WCHAR)+1]){ExFreePool(names);return STATUS_OBJECT_NAME_COLLISION;}
 WDF_OBJECT_ATTRIBUTES_INIT(&a);a.ParentObject=h->device;
 s=WdfIoTargetCreate(h->device,&a,&h->power_target);
 if(NT_SUCCESS(s)){
  WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&open,&name,FILE_READ_ATTRIBUTES);
  open.ShareAccess=FILE_SHARE_READ|FILE_SHARE_WRITE;
  open.EvtIoTargetQueryRemove=query_remove;open.EvtIoTargetRemoveComplete=remove_complete;open.EvtIoTargetRemoveCanceled=remove_canceled;
  s=WdfIoTargetOpen(h->power_target,&open);
 }
 ExFreePool(names);
 if(!NT_SUCCESS(s))goto fail;
 RtlZeroMemory(&h->power,sizeof(h->power));
 s=WdfIoTargetQueryForInterface(h->power_target,&A04SdPowerInterfaceGuid,(PINTERFACE)&h->power,
                              sizeof(h->power),A04_SD_POWER_INTERFACE_VERSION,NULL);
 if(!NT_SUCCESS(s))goto fail;
 h->power_valid=TRUE;
 if(h->power.Header.Size<sizeof(h->power)||h->power.Header.Version!=A04_SD_POWER_INTERFACE_VERSION||
    !h->power.Header.InterfaceDereference||!h->power.Acquire||!h->power.Enable||!h->power.Health||!h->power.ReleaseQuiescent){s=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
 PDEVICE_OBJECT supplier=WdfIoTargetWdmGetTargetPhysicalDevice(h->power_target);
 if(!supplier){s=STATUS_NO_SUCH_DEVICE;goto fail;}
 ObReferenceObject(supplier);ExAcquireFastMutex(&h->power_relations_lock);
 h->supplier=supplier;ExReleaseFastMutex(&h->power_relations_lock);
 /* Microsoft: system S1..S5 supplier ordering is PowerRelations; S0 Dx
  * retention is separately provided by Acquire's matched StopIdle reference. */
 IoInvalidateDeviceRelations(WdfDeviceWdmGetPhysicalDevice(h->device),PowerRelations);
 return STATUS_SUCCESS;
fail:(void)SdPowerDisconnect(h);return s;
}
NTSTATUS SdPowerDisconnect(struct sd_host*h)
{
 if(h->power_token)return STATUS_DEVICE_BUSY;
 if(h->power_valid){
  if(h->power.Header.InterfaceDereference)h->power.Header.InterfaceDereference(h->power.Header.Context);
  h->power_valid=FALSE;RtlZeroMemory(&h->power,sizeof(h->power));
 }
 ExAcquireFastMutex(&h->power_relations_lock);PDEVICE_OBJECT supplier=h->supplier;h->supplier=NULL;
 ExReleaseFastMutex(&h->power_relations_lock);if(supplier)ObDereferenceObject(supplier);
 if(h->power_target){WdfIoTargetClose(h->power_target);WdfObjectDelete(h->power_target);h->power_target=NULL;}
 return STATUS_SUCCESS;
}
NTSTATUS SdPowerHealth(struct sd_host*h)
{
 if(!h->power_valid||!h->power_token)return STATUS_DEVICE_NOT_READY;
 return h->power.Health(h->power.Header.Context,h->power_token);
}
NTSTATUS SdPowerRelease(struct sd_host*h)
{
 NTSTATUS s;if(!h->power_token)return STATUS_SUCCESS;
 s=h->power.ReleaseQuiescent(h->power.Header.Context,h->power_token);
 if(NT_SUCCESS(s))h->power_token=0;
 return s;
}
NTSTATUS SdPowerPnp(WDFDEVICE dev,PIRP irp)
{
 struct sd_host*h=SdHost(dev);PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(irp);
 if(stack->MinorFunction==IRP_MN_DEVICE_USAGE_NOTIFICATION && stack->Parameters.UsageNotification.InPath){
  irp->IoStatus.Status=STATUS_NOT_SUPPORTED;irp->IoStatus.Information=0;IoCompleteRequest(irp,IO_NO_INCREMENT);return STATUS_NOT_SUPPORTED;
 }
 if(stack->MinorFunction==IRP_MN_QUERY_DEVICE_RELATIONS&&stack->Parameters.QueryDeviceRelations.Type==PowerRelations){
  ExAcquireFastMutex(&h->power_relations_lock);PDEVICE_OBJECT supplier=h->supplier;
  if(supplier)ObReferenceObject(supplier);ExReleaseFastMutex(&h->power_relations_lock);
  if(!supplier)goto forward;
  PDEVICE_RELATIONS old=(PDEVICE_RELATIONS)irp->IoStatus.Information,r;ULONG n=old?old->Count:0;
  for(ULONG i=0;i<n;i++)if(old->Objects[i]==supplier){ObDereferenceObject(supplier);goto forward;}
  if(n>(MAXULONG-sizeof(DEVICE_RELATIONS))/sizeof(PDEVICE_OBJECT)){
   ObDereferenceObject(supplier);irp->IoStatus.Status=STATUS_INTEGER_OVERFLOW;IoCompleteRequest(irp,IO_NO_INCREMENT);return STATUS_INTEGER_OVERFLOW;
  }
  r=ExAllocatePoolWithTag(PagedPool,sizeof(DEVICE_RELATIONS)+n*sizeof(PDEVICE_OBJECT),SD_TAG);
  if(!r){ObDereferenceObject(supplier);irp->IoStatus.Status=STATUS_INSUFFICIENT_RESOURCES;IoCompleteRequest(irp,IO_NO_INCREMENT);return STATUS_INSUFFICIENT_RESOURCES;}
  r->Count=n+1;for(ULONG i=0;i<n;i++)r->Objects[i]=old->Objects[i];
  r->Objects[n]=supplier; /* Transfer the reference to PnP. */
  if(old)ExFreePool(old);irp->IoStatus.Information=(ULONG_PTR)r;irp->IoStatus.Status=STATUS_SUCCESS;
 }
forward:IoSkipCurrentIrpStackLocation(irp);return WdfDeviceWdmDispatchPreprocessedIrp(dev,irp);
}
