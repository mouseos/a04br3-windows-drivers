/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Existing ACCDET1102 IOCTL consumer only. No GPIO/PMIC/AFE writes or owners.
 * No request contains an audio miniport/stack-buffer pointer after timeout. */
#ifndef MT8167_AFE_JACK_SENSE_H
#define MT8167_AFE_JACK_SENSE_H
#include "../../input/mt8167-accdet/ioctl.h"
#define JACK_TAG 'jAuM'
static volatile LONG JackRequestActive;
struct Mt8167JackRequest {
    volatile LONG refs;
    KEVENT done;
    PIRP irp;
    PFILE_OBJECT file;
    IO_STATUS_BLOCK result;
    struct mt8167_accdet_socket sample;
};
static void Mt8167JackRelease(Mt8167JackRequest *request)
{
    if(InterlockedDecrement(&request->refs))return;
    IoFreeIrp(request->irp);
    /* Completion may run at DISPATCH_LEVEL; defer possible file deletion. */
    ObDereferenceObjectDeferDelete(request->file);
    ExFreePoolWithTag(request,JACK_TAG);
    InterlockedExchange(&JackRequestActive,0);
}
static NTSTATUS Mt8167JackComplete(PDEVICE_OBJECT,PIRP irp,PVOID context)
{
    Mt8167JackRequest *request=(Mt8167JackRequest *)context;
    request->result=irp->IoStatus;
    KeSetEvent(&request->done,IO_NO_INCREMENT,FALSE);
    Mt8167JackRelease(request);
    return STATUS_MORE_PROCESSING_REQUIRED;
}
static NTSTATUS Mt8167ReadParentJack(PDEVICE_OBJECT owner,struct mt8167_accdet_socket *sample)
{
    PWSTR paths=NULL;PFILE_OBJECT file=NULL;PDEVICE_OBJECT target=NULL;
    UNICODE_STRING path;NTSTATUS status;Mt8167JackRequest *request;
    if(!sample)return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(sample,sizeof(*sample));
    if(!owner||KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
    if(InterlockedCompareExchange(&JackRequestActive,1,0))return STATUS_DEVICE_BUSY;
    status=IoGetDeviceInterfaces(&GUID_MT8167_ACCDET_SOCKET,NULL,0,&paths);
    if(!NT_SUCCESS(status))goto unlock;
    if(!paths||!*paths){status=STATUS_DEVICE_NOT_READY;goto freePaths;}
    RtlInitUnicodeString(&path,paths);
    /* Never select an arbitrary device when more than one parent is enabled. */
    if(paths[path.Length/sizeof(WCHAR)+1]){status=STATUS_DEVICE_NOT_READY;goto freePaths;}
    status=IoGetDeviceObjectPointer(&path,FILE_READ_DATA,&file,&target);
freePaths:
    if(paths)ExFreePool(paths);
    if(!NT_SUCCESS(status))goto unlock;
    request=(Mt8167JackRequest *)ExAllocatePoolWithTag(NonPagedPool,sizeof(*request),JACK_TAG);
    if(!request){ObDereferenceObject(file);status=STATUS_INSUFFICIENT_RESOURCES;goto unlock;}
    RtlZeroMemory(request,sizeof(*request));request->refs=2;request->file=file;
    KeInitializeEvent(&request->done,NotificationEvent,FALSE);
    request->irp=IoAllocateIrp(target->StackSize,FALSE);
    if(!request->irp){ExFreePoolWithTag(request,JACK_TAG);ObDereferenceObject(file);status=STATUS_INSUFFICIENT_RESOURCES;goto unlock;}
    request->irp->RequestorMode=KernelMode;
    request->irp->AssociatedIrp.SystemBuffer=&request->sample;
    request->irp->Flags=IRP_BUFFERED_IO;
    request->irp->IoStatus.Status=STATUS_NOT_SUPPORTED;
    request->irp->IoStatus.Information=0;
    {
        PIO_STACK_LOCATION stack=IoGetNextIrpStackLocation(request->irp);
        /* IoAllocateIrp does not promise zeroed/recycled stack storage. */
        RtlZeroMemory(stack,sizeof(*stack));
        stack->MajorFunction=IRP_MJ_DEVICE_CONTROL;stack->FileObject=file;
        stack->Parameters.DeviceIoControl.IoControlCode=IOCTL_MT8167_ACCDET_READ_SOCKET;
        stack->Parameters.DeviceIoControl.OutputBufferLength=sizeof(request->sample);
        /* Ex holds the caller's driver until completion has returned. */
        status=IoSetCompletionRoutineEx(owner,request->irp,Mt8167JackComplete,request,TRUE,TRUE,TRUE);
    }
    if(!NT_SUCCESS(status)){request->refs=1;Mt8167JackRelease(request);return status;}
    /* A completion is guaranteed after installing the Ex callback. No intervening failure path. */
    IoCallDriver(target,request->irp);
    {
        LARGE_INTEGER timeout;timeout.QuadPart=-1000000; /* 100ms; no polling/retry */
        status=KeWaitForSingleObject(&request->done,Executive,KernelMode,FALSE,&timeout);
        if(status==STATUS_TIMEOUT){
            /* Caller ref keeps the IRP alive while cancel races completion.
             * Late completion owns heap output/file only; no audio context. */
            IoCancelIrp(request->irp);status=STATUS_IO_TIMEOUT;
        }else if(status==STATUS_SUCCESS){
            status=request->result.Status;
            if(NT_SUCCESS(status)){
                if(request->result.Information!=sizeof(*sample))status=STATUS_DEVICE_NOT_READY;
                else RtlCopyMemory(sample,&request->sample,sizeof(*sample));
            }
        }
    }
    Mt8167JackRelease(request);return status;
unlock:
    InterlockedExchange(&JackRequestActive,0);return status;
}
#endif
