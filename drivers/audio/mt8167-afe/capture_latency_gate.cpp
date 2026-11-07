/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <portcls.h>
/* Capture matched profile only. The old render also has no measured latency.
 * WaveRT's void GetHWLatency cannot return an unsupported status. Reject the
 * exact optional KS query before PortCls instead of exporting guessed zeros.
 * Other standard KS properties and actual DMA streaming remain PortCls-owned.
 */
extern "C" NTSTATUS Mt8167LatencyPropertyGate(PIRP irp,BOOLEAN *handled) {
 PIO_STACK_LOCATION sp=IoGetCurrentIrpStackLocation(irp);KSPROPERTY p;
 *handled=FALSE;
 if(sp->Parameters.DeviceIoControl.IoControlCode!=IOCTL_KS_PROPERTY||
    sp->Parameters.DeviceIoControl.InputBufferLength<sizeof(p))return STATUS_SUCCESS;
 /* METHOD_NEITHER: snapshot caller memory through the Windows 8.1 safe
  * copy DDI. ARM clang cannot emit kernel SEH; never dereference a user
  * Type3InputBuffer or substitute a ProbeForRead call without a handler. */
 if(KeGetCurrentIrql()>APC_LEVEL){*handled=TRUE;return STATUS_INVALID_DEVICE_STATE;}
 MM_COPY_ADDRESS source;SIZE_T copied=0;
 source.VirtualAddress=sp->Parameters.DeviceIoControl.Type3InputBuffer;
 NTSTATUS status=MmCopyMemory(&p,source,sizeof(p),MM_COPY_MEMORY_VIRTUAL,&copied);
 if(!NT_SUCCESS(status)||copied!=sizeof(p)){
  *handled=TRUE;return NT_SUCCESS(status)?STATUS_INVALID_USER_BUFFER:status;
 }
 if(IsEqualGUIDAligned(p.Set,KSPROPSETID_RtAudio)&&p.Id==KSPROPERTY_RTAUDIO_HWLATENCY){
  *handled=TRUE;return STATUS_NOT_SUPPORTED;
 }
 return STATUS_SUCCESS;
}
