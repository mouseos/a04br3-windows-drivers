/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_owner_client.h"
static NTSTATUS complete(PDEVICE_OBJECT dev,PIRP irp,PVOID context)
{UNREFERENCED_PARAMETER(dev);UNREFERENCED_PARAMETER(irp);KeSetEvent((PKEVENT)context,IO_NO_INCREMENT,FALSE);return STATUS_MORE_PROCESSING_REQUIRED;}
static NTSTATUS query(PDEVICE_OBJECT target,const GUID *type,USHORT size,PINTERFACE out)
{
 KEVENT event;PIRP irp;PIO_STACK_LOCATION stack;NTSTATUS status;
 irp=IoAllocateIrp(target->StackSize,FALSE);if(!irp)return STATUS_INSUFFICIENT_RESOURCES;
 KeInitializeEvent(&event,NotificationEvent,FALSE);irp->IoStatus.Status=STATUS_NOT_SUPPORTED;
 stack=IoGetNextIrpStackLocation(irp);stack->MajorFunction=IRP_MJ_PNP;stack->MinorFunction=IRP_MN_QUERY_INTERFACE;
 stack->Parameters.QueryInterface.InterfaceType=type;
 stack->Parameters.QueryInterface.Size=size;stack->Parameters.QueryInterface.Version=A04_I2C1_IDENTITY_VERSION;
 stack->Parameters.QueryInterface.Interface=(PINTERFACE)out;stack->Parameters.QueryInterface.InterfaceSpecificData=NULL;
 IoSetCompletionRoutine(irp,complete,&event,TRUE,TRUE,TRUE);status=IoCallDriver(target,irp);
 if(status==STATUS_PENDING)KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);
 status=irp->IoStatus.Status;IoFreeIrp(irp);return status;
}
NTSTATUS a04_hi556_owner_open(A04_I2C1_IDENTITY_INTERFACE *out)
{
 PWSTR paths=NULL;UNICODE_STRING path;PFILE_OBJECT file=NULL;PDEVICE_OBJECT target=NULL;NTSTATUS status;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!out)return STATUS_INVALID_DEVICE_STATE;
 RtlZeroMemory(out,sizeof(*out));status=IoGetDeviceInterfaces(&A04I2c1OwnerDeviceGuid,NULL,0,&paths);
 if(!NT_SUCCESS(status))return status;
 if(!paths[0]||paths[wcslen(paths)+1]){status=STATUS_NOINTERFACE;goto done;}
 RtlInitUnicodeString(&path,paths);status=IoGetDeviceObjectPointer(&path,FILE_READ_ATTRIBUTES,&file,&target);
 if(!NT_SUCCESS(status))goto done;
 status=query(target,&A04I2c1IdentityInterfaceGuid,sizeof(*out),(PINTERFACE)out);ObDereferenceObject(file);
 if(NT_SUCCESS(status)&& (out->Header.Version!=A04_I2C1_IDENTITY_VERSION||out->Header.Size<sizeof(*out)||
    !out->Header.InterfaceReference||!out->Header.InterfaceDereference||!out->Identify)){
  if(out->Header.InterfaceDereference)out->Header.InterfaceDereference(out->Header.Context);
  RtlZeroMemory(out,sizeof(*out));status=STATUS_NOINTERFACE;
 }
done:ExFreePool(paths);return status;
}
void a04_hi556_owner_close(A04_I2C1_IDENTITY_INTERFACE *p)
{
 if(p&&KeGetCurrentIrql()==PASSIVE_LEVEL){
  if(p->Header.InterfaceDereference)p->Header.InterfaceDereference(p->Header.Context);
  RtlZeroMemory(p,sizeof(*p));
 }
}
NTSTATUS a04_hi556_owner_identify(A04_I2C1_IDENTITY_INTERFACE *p,const A04_HI556_POWER_INTERFACE *power,A04_HI556_IDENTITY_RESULT *out)
{
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!p||!p->Identify||!p->Header.InterfaceDereference)return STATUS_INVALID_DEVICE_STATE;
 return p->Identify(p->Header.Context,power,out);
}

NTSTATUS a04_hi556_camera_open(A04_I2C1_CAMERA_INTERFACE *out)
{
 PWSTR paths=NULL;UNICODE_STRING path;PFILE_OBJECT file=NULL;PDEVICE_OBJECT target=NULL;NTSTATUS status;
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL||!out)return STATUS_INVALID_DEVICE_STATE;
 RtlZeroMemory(out,sizeof(*out));status=IoGetDeviceInterfaces(&A04I2c1OwnerDeviceGuid,NULL,0,&paths);if(!NT_SUCCESS(status))return status;
 if(!paths[0]||paths[wcslen(paths)+1]){status=STATUS_NOINTERFACE;goto done;}
 RtlInitUnicodeString(&path,paths);status=IoGetDeviceObjectPointer(&path,FILE_READ_ATTRIBUTES,&file,&target);if(!NT_SUCCESS(status))goto done;
 status=query(target,&A04I2c1CameraRegisterGuid,sizeof(*out),(PINTERFACE)out);ObDereferenceObject(file);
 if(NT_SUCCESS(status)&&(out->Header.Version!=1||out->Header.Size<sizeof(*out)||!out->Header.InterfaceDereference||!out->Register)){
  if(out->Header.InterfaceDereference)out->Header.InterfaceDereference(out->Header.Context);
  RtlZeroMemory(out,sizeof(*out));status=STATUS_NOINTERFACE;
 }
done:ExFreePool(paths);return status;
}
void a04_hi556_camera_close(A04_I2C1_CAMERA_INTERFACE *p)
{if(p&&KeGetCurrentIrql()==PASSIVE_LEVEL){if(p->Header.InterfaceDereference)p->Header.InterfaceDereference(p->Header.Context);RtlZeroMemory(p,sizeof(*p));}}
