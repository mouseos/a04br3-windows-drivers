/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include <ntddk.h>
#include <portcls.h>
/* Unknown latency must not become a successful zero-valued response through
 * WaveRT's void GetHWLatency. This gate currently blocks ALL WaveRT WASAPI
 * initialization, including speaker: RT8.1 AudioKSE 0x1000a3ac propagates the
 * mandatory HWLATENCY (property2) failure after buffer allocation. Only the
 * subsequent POSITIONREGISTER (property3) has an ERROR_NOT_SUPPORTED fallback.
 * Keep rejection until a qualified FIFO/chipset/codec latency producer exists;
 * removing it is not a playback fix. See RUN143-LATENCY-CONTRACT.md.
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
