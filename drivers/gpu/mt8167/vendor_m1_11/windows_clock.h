/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_WINDOWS_GPU_CLOCK_H
#define MT8167_WINDOWS_GPU_CLOCK_H
#include <ntddk.h>
#include "../rogue_power.h"
/* Volatile firmware snapshot, not a clock ownership lease. No platform MMIO. */
struct mt8167_gpu_clock_record {
    ULONG version,bytes,parent,clock_hz,con0,con1,power;
    ULONG rail_control,rail_status,selector0,selector1;
};
static NTSTATUS mt8167_gpu_clock_read(ULONG *hz,ULONG *parent)
{
    UNICODE_STRING name=RTL_CONSTANT_STRING(L"Mt8167GpuClock");
    GUID guid={0x9c50bc94,0x6c80,0x4fa2,{0x93,0x9d,0x92,0x78,0xd0,0x7f,0x70,0x10}};
    struct mt8167_gpu_clock_record r;
    ULONG size=sizeof(r),attributes=0,validated;NTSTATUS status;
    if(!hz || !parent || KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    *hz=26000000u;*parent=4;
    RtlZeroMemory(&r,sizeof(r));
    status=ExGetFirmwareEnvironmentVariable(&name,&guid,&r,&size,&attributes);
    /* Older crystal-only firmware has no record. An invalid present record
     * must not silently authorize a mismatched firmware clock. */
    if(status==STATUS_VARIABLE_NOT_FOUND || status==STATUS_NOT_IMPLEMENTED) return STATUS_SUCCESS;
    if(!NT_SUCCESS(status)) return status;
    if(size!=sizeof(r) || r.version!=1 || r.bytes!=sizeof(r) || attributes!=6 ||
       r.parent!=7 || (r.rail_control|r.rail_status|r.selector0|r.selector1)>0xffffu)
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    validated=a04_rgx_mmpll_hz(r.con0,r.con1,r.power,(uint16_t)r.rail_control,
        (uint16_t)r.rail_status,(uint16_t)r.selector0,(uint16_t)r.selector1);
    if(!validated || validated!=r.clock_hz) return STATUS_DEVICE_CONFIGURATION_ERROR;
    *hz=validated;*parent=r.parent;return STATUS_SUCCESS;
}
#endif
