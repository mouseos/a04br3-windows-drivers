/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "windows_msdc2.h"
static uint32_t reg_read(void *context,unsigned off,unsigned width)
{
 struct a04_windows_msdc2 *m=context;
 if(!m->base || off>0xfffu || (width!=1 && width!=4) || width>0x1000u-off || (width==4 && (off&3)))return 0xffffffffu;
 return width==1?READ_REGISTER_UCHAR(m->base+off):READ_REGISTER_ULONG((volatile ULONG *)(m->base+off));
}
static void reg_write(void *context,unsigned off,uint32_t value,unsigned width)
{
 struct a04_windows_msdc2 *m=context;
 if(!m->base || off>0xfffu || (width!=1 && width!=4) || width>0x1000u-off || (width==4 && (off&3)))return;
 if(width==1)WRITE_REGISTER_UCHAR(m->base+off,(UCHAR)value);
 else WRITE_REGISTER_ULONG((volatile ULONG *)(m->base+off),value);
}
static void delay(void *context,unsigned us)
{
 LARGE_INTEGER interval;UNREFERENCED_PARAMETER(context);
 /* Enumeration can request 10 ms. Do not busy-stall the CPU for that long.
  * This component is restricted to PASSIVE_LEVEL by its caller contract. */
 if(us<1000){KeStallExecutionProcessor(us);return;}
 interval.QuadPart=-(LONGLONG)us*10;
 (void)KeDelayExecutionThread(KernelMode,FALSE,&interval);
}
NTSTATUS a04_windows_msdc2_map_prepare(struct a04_windows_msdc2 *m,
 const CM_PARTIAL_RESOURCE_DESCRIPTOR *raw,const CM_PARTIAL_RESOURCE_DESCRIPTOR *trans,
 struct a04_wifi_owner *owner,unsigned generation)
{
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL || !m || m->base || !raw || !trans || !owner ||
    owner->poisoned || !owner->active || owner->generation!=generation ||
    raw->Type!=CmResourceTypeMemory || trans->Type!=CmResourceTypeMemory ||
    raw->ShareDisposition!=CmResourceShareDeviceExclusive || trans->ShareDisposition!=CmResourceShareDeviceExclusive ||
    raw->u.Memory.Start.QuadPart!=0x11170000 || raw->u.Memory.Length!=0x1000 || trans->u.Memory.Length!=0x1000)
  return STATUS_DEVICE_NOT_READY;
 m->base=MmMapIoSpace(trans->u.Memory.Start,0x1000,MmNonCached);
 if(!m->base)return STATUS_INSUFFICIENT_RESOURCES;
 return STATUS_SUCCESS;
}
NTSTATUS a04_windows_msdc2_activate(struct a04_windows_msdc2 *m,struct a04_wifi_owner *owner,unsigned generation,int irq_quiesced)
{
 if(KeGetCurrentIrql()!=PASSIVE_LEVEL || !m || !m->base || m->pio.owner || !owner ||
    !owner->active || owner->poisoned || owner->generation!=generation || !irq_quiesced)return STATUS_DEVICE_NOT_READY;
 m->pio.context=m;m->pio.read=reg_read;m->pio.write=reg_write;m->pio.delay=delay;
 m->pio.owner=owner;m->pio.generation=generation;m->pio.poisoned=0;
 return STATUS_SUCCESS;
}
NTSTATUS a04_windows_msdc2_map(struct a04_windows_msdc2 *m,const CM_PARTIAL_RESOURCE_DESCRIPTOR *raw,
 const CM_PARTIAL_RESOURCE_DESCRIPTOR *trans,struct a04_wifi_owner *owner,unsigned generation,int irq_quiesced)
{
 NTSTATUS status;
 if(!irq_quiesced)return STATUS_DEVICE_NOT_READY;
 status=a04_windows_msdc2_map_prepare(m,raw,trans,owner,generation);
 if(!NT_SUCCESS(status))return status;
 status=a04_windows_msdc2_activate(m,owner,generation,irq_quiesced);
 if(!NT_SUCCESS(status))a04_windows_msdc2_unmap(m);
 return status;
}
void a04_windows_msdc2_unmap(struct a04_windows_msdc2 *m)
{
 if(!m || !m->base)return;
 /* Caller has stopped requests and drained IRQ/PIO; no reset or power writes. */
 m->pio.poisoned=1;MmUnmapIoSpace((PVOID)m->base,0x1000);m->base=NULL;
}
