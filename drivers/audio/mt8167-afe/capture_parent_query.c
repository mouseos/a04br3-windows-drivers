#include "../../bus/mt8167-shared/capture_interface.h"
static NTSTATUS CaptureQueryComplete(PDEVICE_OBJECT device,PIRP irp,PVOID context)
{
    UNREFERENCED_PARAMETER(device);UNREFERENCED_PARAMETER(irp);
    KeSetEvent((PKEVENT)context,IO_NO_INCREMENT,FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}
NTSTATUS Mt8167QueryCaptureParent(PDEVICE_OBJECT target,A04_CAPTURE_INTERFACE *out)
{
    KEVENT event;PIRP irp;PIO_STACK_LOCATION stack;NTSTATUS status;
    if(!target || !out || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    RtlZeroMemory(out,sizeof(*out));KeInitializeEvent(&event,NotificationEvent,FALSE);
    irp=IoAllocateIrp(target->StackSize,FALSE);if(!irp)return STATUS_INSUFFICIENT_RESOURCES;
    irp->IoStatus.Status=STATUS_NOT_SUPPORTED;irp->IoStatus.Information=0;
    stack=IoGetNextIrpStackLocation(irp);stack->MajorFunction=IRP_MJ_PNP;stack->MinorFunction=IRP_MN_QUERY_INTERFACE;
    stack->Parameters.QueryInterface.InterfaceType=&A04CaptureInterfaceGuid;
    stack->Parameters.QueryInterface.Size=sizeof(*out);stack->Parameters.QueryInterface.Version=A04_CAPTURE_INTERFACE_VERSION;
    stack->Parameters.QueryInterface.Interface=(PINTERFACE)out;stack->Parameters.QueryInterface.InterfaceSpecificData=NULL;
    IoSetCompletionRoutine(irp,CaptureQueryComplete,&event,TRUE,TRUE,TRUE);
    status=IoCallDriver(target,irp);
    if(status==STATUS_PENDING) KeWaitForSingleObject(&event,Executive,KernelMode,FALSE,NULL);
    status=irp->IoStatus.Status;IoFreeIrp(irp);
    if(!NT_SUCCESS(status)) RtlZeroMemory(out,sizeof(*out));
    return status;
}
