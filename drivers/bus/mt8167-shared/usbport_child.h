/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_USBPORT_CHILD_H
#define A04_USBPORT_CHILD_H
/* Opt-in platform-description support for the synthesized MT8167\USB20 PDO.
 * These fields describe this bus-created USB function, not invented ACPI
 * firmware methods or silicon revision. No clock/power ownership is granted. */
#ifndef A04_SHARED_USBPORT_CHILD
#define A04_SHARED_USBPORT_CHILD 0
#endif
#if A04_SHARED_USBPORT_CHILD
#include <ntddk.h>
#include <wdf.h>
#include <acpiioct.h>
EVT_WDFDEVICE_WDM_IRP_PREPROCESS A04UsbPortChildIoctl;
static NTSTATUS A04UsbPortChildInformation(PIRP irp)
{
 PIO_STACK_LOCATION s=IoGetCurrentIrpStackLocation(irp);
 PACPI_DEVICE_INFORMATION_OUTPUT_BUFFER d;ULONG need=sizeof(*d)+sizeof("MT8167\\USB20")+sizeof("0");
 PUCHAR b=(PUCHAR)irp->AssociatedIrp.SystemBuffer;
 if(s->Parameters.DeviceIoControl.OutputBufferLength<need){irp->IoStatus.Information=need;return STATUS_BUFFER_TOO_SMALL;}
 if(!b)return STATUS_INVALID_PARAMETER;
 RtlZeroMemory(b,need);d=(PACPI_DEVICE_INFORMATION_OUTPUT_BUFFER)b;
 d->Signature=IOCTL_ACPI_GET_DEVICE_INFORMATION_SIGNATURE;d->Size=(USHORT)need;d->Revision=1;
 d->VendorIdStringOffset=sizeof(*d);d->VendorStringLength=sizeof("MT8167\\USB20")-1;
 d->DeviceIdStringOffset=sizeof(*d)+7;d->InstanceIdOffset=sizeof(*d)+sizeof("MT8167\\USB20");d->InstanceIdLength=1;
 d->BaseClassCode=0x0c;d->SubClassCode=3;d->ProgrammingInterface=0;
 RtlCopyMemory(b+sizeof(*d),"MT8167\\USB20",sizeof("MT8167\\USB20"));
 RtlCopyMemory(b+d->InstanceIdOffset,"0",sizeof("0"));irp->IoStatus.Information=need;return STATUS_SUCCESS;
}
NTSTATUS A04UsbPortChildIoctl(WDFDEVICE device,PIRP irp)
{
 PIO_STACK_LOCATION s=IoGetCurrentIrpStackLocation(irp);NTSTATUS status;
 if(s->Parameters.DeviceIoControl.IoControlCode!=IOCTL_ACPI_GET_DEVICE_INFORMATION){
  IoSkipCurrentIrpStackLocation(irp);return WdfDeviceWdmDispatchPreprocessedIrp(device,irp);
 }
 irp->IoStatus.Information=0;status=A04UsbPortChildInformation(irp);irp->IoStatus.Status=status;
 IoCompleteRequest(irp,IO_NO_INCREMENT);return status;
}
#endif
#endif
