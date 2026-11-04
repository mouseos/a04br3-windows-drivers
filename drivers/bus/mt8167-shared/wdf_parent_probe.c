/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Non-operational PnP parent prototype. No INF, ACPI registration, interface,
 * interrupt, hardware access or clients. D0 always fails. Never deploy this
 * while existing button/touch drivers claim the same resources. */
#include <ntddk.h>
#include <wdf.h>
#include "parent_resources.h"
typedef struct { PVOID topck,infra,gpio,pwrap; } A04_PARENT_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(A04_PARENT_CONTEXT,ParentContext)
EVT_WDF_DRIVER_DEVICE_ADD ParentAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE ParentPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE ParentRelease;
EVT_WDF_DEVICE_D0_ENTRY ParentD0;
static void ParentUnmap(A04_PARENT_CONTEXT *c)
{
    if(c->pwrap){MmUnmapIoSpace(c->pwrap,0x1000);c->pwrap=NULL;}
    if(c->gpio){MmUnmapIoSpace(c->gpio,0x1000);c->gpio=NULL;}
    if(c->infra){MmUnmapIoSpace(c->infra,0x1000);c->infra=NULL;}
    if(c->topck){MmUnmapIoSpace(c->topck,0x1000);c->topck=NULL;}
}
NTSTATUS ParentPrepare(WDFDEVICE dev,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
    A04_PARENT_CONTEXT *c=ParentContext(dev);struct a04_parent_resource r[4];
    struct a04_parent_mapping mapping;ULONG i,n=0;PHYSICAL_ADDRESS pa;
    UNREFERENCED_PARAMETER(raw);
    if(c->topck||c->infra||c->gpio||c->pwrap)return STATUS_INVALID_DEVICE_STATE;
    for(i=0;i<WdfCmResourceListGetCount(translated);++i) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d=WdfCmResourceListGetDescriptor(translated,i);
        if(!d)return STATUS_DEVICE_CONFIGURATION_ERROR;
        if(d->Type!=CmResourceTypeMemory)continue;
        if(n>=4)return STATUS_DEVICE_CONFIGURATION_ERROR;
        r[n].base=(uint64_t)d->u.Memory.Start.QuadPart;r[n].length=d->u.Memory.Length;
        r[n].exclusive=d->ShareDisposition==CmResourceShareDeviceExclusive ? 1u:0u;++n;
    }
    if(a04_parent_validate_resources(r,n,&mapping))return STATUS_DEVICE_CONFIGURATION_ERROR;
    pa.QuadPart=(LONGLONG)mapping.topck;c->topck=MmMapIoSpace(pa,0x1000,MmNonCached);
    if(!c->topck)goto fail;
    pa.QuadPart=(LONGLONG)mapping.infra;c->infra=MmMapIoSpace(pa,0x1000,MmNonCached);
    if(!c->infra)goto fail;
    pa.QuadPart=(LONGLONG)mapping.gpio;c->gpio=MmMapIoSpace(pa,0x1000,MmNonCached);
    if(!c->gpio)goto fail;
    pa.QuadPart=(LONGLONG)mapping.pwrap;c->pwrap=MmMapIoSpace(pa,0x1000,MmNonCached);
    if(!c->pwrap)goto fail;
    return STATUS_SUCCESS;
fail:
    ParentUnmap(c);return STATUS_INSUFFICIENT_RESOURCES;
}
NTSTATUS ParentRelease(WDFDEVICE dev,WDFCMRESLIST translated)
{
    UNREFERENCED_PARAMETER(translated);
    /* No interfaces or clients are published while D0 is gated. */
    ParentUnmap(ParentContext(dev));return STATUS_SUCCESS;
}
NTSTATUS ParentD0(WDFDEVICE dev,WDF_POWER_DEVICE_STATE previous)
{
    UNREFERENCED_PARAMETER(dev);UNREFERENCED_PARAMETER(previous);
    /* Existing direct owners, boot ordering and shared lease teardown have not
     * been migrated. An ACPI resource assignment alone is not ownership. */
    return STATUS_DEVICE_NOT_READY;
}
NTSTATUS ParentAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;WDF_OBJECT_ATTRIBUTES attr;WDFDEVICE dev;
    UNREFERENCED_PARAMETER(driver);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware=ParentPrepare;pnp.EvtDeviceReleaseHardware=ParentRelease;pnp.EvtDeviceD0Entry=ParentD0;
    WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,A04_PARENT_CONTEXT);
    attr.ExecutionLevel=WdfExecutionLevelPassive;
    return WdfDeviceCreate(&init,&attr,&dev);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry)
{
    WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,ParentAdd);
    return WdfDriverCreate(driver,registry,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
