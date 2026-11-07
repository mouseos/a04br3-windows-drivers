/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <ntddk.h>
#include <ks.h>
#include <ksmedia.h>
/* Capture matched profile only. The old render also has no measured latency.
 * WaveRT's void GetHWLatency cannot return an unsupported status. Reject the
 * exact optional KS query before PortCls instead of exporting guessed zeros.
 * Other standard KS properties and actual DMA streaming remain PortCls-owned.
 */
NTSTATUS Mt8167LatencyPropertyGate(PIRP irp,BOOLEAN *handled) {
 PIO_STACK_LOCATION sp=IoGetCurrentIrpStackLocation(irp);KSPROPERTY p;
 *handled=FALSE;
 if(sp->Parameters.DeviceIoControl.IoControlCode!=IOCTL_KS_PROPERTY||
    sp->Parameters.DeviceIoControl.InputBufferLength<sizeof(p))return STATUS_SUCCESS;
 __try {
  PVOID in=sp->Parameters.DeviceIoControl.Type3InputBuffer;
  if(irp->RequestorMode!=KernelMode)ProbeForRead(in,sizeof(p),1);
  RtlCopyMemory(&p,in,sizeof(p));
 }__except(EXCEPTION_EXECUTE_HANDLER){*handled=TRUE;return GetExceptionCode();}
 if(IsEqualGUID(&p.Set,&KSPROPSETID_RtAudio)&&p.Id==KSPROPERTY_RTAUDIO_HWLATENCY){
  *handled=TRUE;return STATUS_NOT_SUPPORTED;
 }
 return STATUS_SUCCESS;
}
