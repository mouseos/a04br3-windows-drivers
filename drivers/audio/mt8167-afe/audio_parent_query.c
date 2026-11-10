#include "audio_parent_query.h"
static NTSTATUS AudioQueryComplete(PDEVICE_OBJECT device,PIRP irp,PVOID context)
{
    UNREFERENCED_PARAMETER(device);UNREFERENCED_PARAMETER(irp);
    KeSetEvent((PKEVENT)context,IO_NO_INCREMENT,FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}
static NTSTATUS QueryAudioInterface(PDEVICE_OBJECT target,PVOID out,USHORT bytes,USHORT version,const GUID *guid)
{
    KEVENT event;PIRP irp;PIO_STACK_LOCATION stack;NTSTATUS status;
    if(!target || !out || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(out,bytes);KeInitializeEvent(&event,NotificationEvent,FALSE);
    irp=IoAllocateIrp(target->StackSize,FALSE);if(!irp)return STATUS_INSUFFICIENT_RESOURCES;
    irp->IoStatus.Status=STATUS_NOT_SUPPORTED;
    stack=IoGetNextIrpStackLocation(irp);stack->MajorFunction=IRP_MJ_PNP;stack->MinorFunction=IRP_MN_QUERY_INTERFACE;
    stack->Parameters.QueryInterface.InterfaceType=guid;
    stack->Parameters.QueryInterface.Size=bytes;stack->Parameters.QueryInterface.Version=version;
    stack->Parameters.QueryInterface.Interface=(PINTERFACE)out;stack->Parameters.QueryInterface.InterfaceSpecificData=NULL;
    IoSetCompletionRoutine(irp,AudioQueryComplete,&event,TRUE,TRUE,TRUE);
    status=IoCallDriver(target,irp);
    if(status==STATUS_PENDING) KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);
    status=irp->IoStatus.Status;IoFreeIrp(irp);
    if(!NT_SUCCESS(status)) RtlZeroMemory(out,bytes);
    return status;
}

NTSTATUS Mt8167QueryAudioParent(PDEVICE_OBJECT target,A04_SHARED_AUDIO_INTERFACE *out)
{return QueryAudioInterface(target,out,sizeof(*out),A04_SHARED_AUDIO_INTERFACE_VERSION,&A04SharedAudioInterfaceGuid);}
#if defined(A04_AUDIO_LIFECYCLE) && A04_AUDIO_LIFECYCLE
NTSTATUS Mt8167QueryAudioLifecycle(PDEVICE_OBJECT target,struct A04_AUDIO_LIFECYCLE_INTERFACE *out)
{return QueryAudioInterface(target,out,sizeof(*out),A04_AUDIO_LIFECYCLE_VERSION,&A04AudioLifecycleGuid);}
#endif
