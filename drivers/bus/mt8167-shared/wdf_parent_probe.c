/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Non-operational PnP parent prototype. No INF, ACPI registration, interrupt,
 * active hardware access or clients. Optional offline query interface only.
 * D0 always fails. Never deploy this
 * while existing button/touch drivers claim the same resources. */
#include <initguid.h>
#include <ntddk.h>
#include <wdf.h>
#include "parent_resources.h"
#include "pwrap_owner.h"
#include "register_owner.h"
#include "client_interface.h"
#if defined(A04_SHARED_QUERY_EXPERIMENTAL) && A04_SHARED_QUERY_EXPERIMENTAL
#include "shared_interface.h"
#endif
typedef struct { PVOID topck,infra,gpio,pwrap; FAST_MUTEX pwrapLock; struct a04_pwrap_owner pmic; struct a04_register_owner regs; } A04_PARENT_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(A04_PARENT_CONTEXT,ParentContext)
EVT_WDF_DRIVER_DEVICE_ADD ParentAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE ParentPrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE ParentRelease;
EVT_WDF_DEVICE_D0_ENTRY ParentD0;
EVT_WDF_DEVICE_D0_EXIT ParentD0Exit;
EVT_WDF_DEVICE_SURPRISE_REMOVAL ParentSurprise;
static int ParentLock(void *context) { ExAcquireFastMutex((FAST_MUTEX *)context);return 0; }
static void ParentUnlock(void *context) { ExReleaseFastMutex((FAST_MUTEX *)context); }
static uint32_t ParentPwrapRead(void *context,unsigned offset) {
    A04_PARENT_CONTEXT *c=(A04_PARENT_CONTEXT *)context;
    return READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->pwrap+offset));
}
static void ParentPwrapWrite(void *context,unsigned offset,uint32_t value) {
    A04_PARENT_CONTEXT *c=(A04_PARENT_CONTEXT *)context;
    WRITE_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)c->pwrap+offset),value);
}
static void ParentPwrapDelay(void *context) { UNREFERENCED_PARAMETER(context);KeStallExecutionProcessor(1); }
static int ParentRegRead(void *context,enum a04_shared_bank bank,unsigned offset,uint32_t *value)
{
    A04_PARENT_CONTEXT *c=context; PVOID base;
    if (!value || offset>0xffcu || (offset&3)) return -1;
    base=bank==A04_SHARED_TOP?c->topck:bank==A04_SHARED_INFRA?c->infra:bank==A04_SHARED_GPIO?c->gpio:NULL;
    if(!base)return -1;
    *value=READ_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)base+offset));return 0;
}
static int ParentRegWrite(void *context,enum a04_shared_bank bank,unsigned offset,uint32_t value)
{
    A04_PARENT_CONTEXT *c=context; PVOID base;
    if(offset>0xffcu || (offset&3))return -1;
    base=bank==A04_SHARED_TOP?c->topck:bank==A04_SHARED_INFRA?c->infra:bank==A04_SHARED_GPIO?c->gpio:NULL;
    if(!base)return -1;
    WRITE_REGISTER_ULONG((volatile ULONG *)((volatile UCHAR *)base+offset),value);return 0;
}
static int ParentRegLock(void *context) { return ParentLock(&((A04_PARENT_CONTEXT *)context)->pwrapLock); }
static void ParentRegUnlock(void *context) { ParentUnlock(&((A04_PARENT_CONTEXT *)context)->pwrapLock); }
#if defined(A04_SHARED_QUERY_EXPERIMENTAL) && A04_SHARED_QUERY_EXPERIMENTAL
static int ParentBank(unsigned bank,enum a04_shared_bank *out)
{
    if(bank==A04_INTERFACE_TOUCH_GPIO)*out=A04_SHARED_GPIO;
    else if(bank==A04_INTERFACE_TOUCH_TOP)*out=A04_SHARED_TOP;
    else if(bank==A04_INTERFACE_TOUCH_INFRA)*out=A04_SHARED_INFRA;
    else return -1;
    return 0;
}
static int ParentTouchUpdate(PVOID context,unsigned bank,unsigned off,ULONG mask,ULONG bits)
{
    enum a04_shared_bank b;
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL || ParentBank(bank,&b))return -1;
    return a04_register_touch_update(&ParentContext((WDFDEVICE)context)->regs,b,off,mask,bits);
}
static int ParentTouchWrite(PVOID context,unsigned bank,unsigned off,ULONG value)
{
    enum a04_shared_bank b;
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL || ParentBank(bank,&b))return -1;
    return a04_register_touch_write(&ParentContext((WDFDEVICE)context)->regs,b,off,value);
}
static int ParentTouchPen(PVOID context,int *down)
{
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    return a04_register_touch_input(&ParentContext((WDFDEVICE)context)->regs,down);
}
static int ParentButtonRead(PVOID context,USHORT *value)
{
    if(!context || KeGetCurrentIrql()!=PASSIVE_LEVEL)return -1;
    return a04_pwrap_owner_read_buttons(&ParentContext((WDFDEVICE)context)->pmic,value);
}
static VOID ParentInterfaceReference(PVOID context) { WdfObjectReference((WDFDEVICE)context); }
static VOID ParentInterfaceDereference(PVOID context) { WdfObjectDereference((WDFDEVICE)context); }
static NTSTATUS ParentReadButtons(PVOID context,USHORT *value) {
    A04_PARENT_CONTEXT *c;
    if(!context || !value || KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_PARAMETER;
    c=ParentContext((WDFDEVICE)context);
    return a04_pwrap_owner_read_buttons(&c->pmic,value)==0 ? STATUS_SUCCESS:STATUS_DEVICE_NOT_READY;
}
#endif
static void ParentStop(A04_PARENT_CONTEXT *c)
{
    if(c->regs.lock) (void)a04_register_owner_stop(&c->regs);
    if(c->pmic.lock) (void)a04_pwrap_owner_stop(&c->pmic);
}
static void ParentUnmap(A04_PARENT_CONTEXT *c)
{
    ParentStop(c);
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
    /* Ownership remains offline: D0 never starts it. */
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
VOID ParentSurprise(WDFDEVICE dev)
{
    /* Offline admission only: this cannot prevent a bus abort from removal
     * concurrent with an already executing hardware access. */
    ParentStop(ParentContext(dev));
}
NTSTATUS ParentD0Exit(WDFDEVICE dev,WDF_POWER_DEVICE_STATE target)
{
    UNREFERENCED_PARAMETER(target);ParentStop(ParentContext(dev));return STATUS_SUCCESS;
}
NTSTATUS ParentD0(WDFDEVICE dev,WDF_POWER_DEVICE_STATE previous)
{
    UNREFERENCED_PARAMETER(dev);UNREFERENCED_PARAMETER(previous);
    /* Existing direct owners, boot ordering and shared lease teardown have not
     * been migrated. An ACPI resource assignment alone is not ownership. */
    return STATUS_DEVICE_NOT_READY;
}
#if defined(A04_SHARED_CHILD_PROBE) && A04_SHARED_CHILD_PROBE
/* Compile-only topology. PnP arbitrates these exclusive child ranges; the
 * parent must not map them. Parent D0 remains gated, and legacy ACPI devices
 * must be disabled in a future mutually exclusive firmware configuration. */
typedef struct { ULONG base; } A04_CHILD_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(A04_CHILD_CONTEXT,ChildContext)
EVT_WDF_DEVICE_RESOURCE_REQUIREMENTS_QUERY ChildRequirements;
NTSTATUS ChildRequirements(WDFDEVICE child,WDFIORESREQLIST requirements)
{
    A04_CHILD_CONTEXT *c=ChildContext(child); WDFIORESLIST list;
    IO_RESOURCE_DESCRIPTOR descriptor; NTSTATUS status;
    if(c->base!=0x10002000u && c->base!=0x11009000u)return STATUS_DEVICE_CONFIGURATION_ERROR;
    status=WdfIoResourceListCreate(requirements,WDF_NO_OBJECT_ATTRIBUTES,&list);
    if(!NT_SUCCESS(status))return status;
    RtlZeroMemory(&descriptor,sizeof(descriptor));
    descriptor.Type=CmResourceTypeMemory; descriptor.ShareDisposition=CmResourceShareDeviceExclusive;
    descriptor.Flags=CM_RESOURCE_MEMORY_READ_WRITE;
    descriptor.u.Memory.Length=0x1000; descriptor.u.Memory.Alignment=1;
    descriptor.u.Memory.MinimumAddress.QuadPart=c->base;
    descriptor.u.Memory.MaximumAddress.QuadPart=(LONGLONG)c->base+0xfff;
    status=WdfIoResourceListAppendDescriptor(list,&descriptor);
    if(!NT_SUCCESS(status)){WdfObjectDelete(list);return status;}
    status=WdfIoResourceRequirementsListAppendIoResList(requirements,list);
    if(!NT_SUCCESS(status))WdfObjectDelete(list);
    return status;
}
static NTSTATUS ParentAddProbeChild(WDFDEVICE parent, PCWSTR id, PCWSTR instance, ULONG base)
{
    PWDFDEVICE_INIT init; WDFDEVICE child; WDF_QUERY_INTERFACE_CONFIG query;
    UNICODE_STRING name; NTSTATUS status; WDF_OBJECT_ATTRIBUTES attr; WDF_PDO_EVENT_CALLBACKS callbacks;
    init=WdfPdoInitAllocate(parent);if(!init)return STATUS_INSUFFICIENT_RESOURCES;
    RtlInitUnicodeString(&name,id);status=WdfPdoInitAssignDeviceID(init,&name);
    if(!NT_SUCCESS(status))goto fail;
    status=WdfPdoInitAddHardwareID(init,&name);if(!NT_SUCCESS(status))goto fail;
    RtlInitUnicodeString(&name,instance);status=WdfPdoInitAssignInstanceID(init,&name);
    if(!NT_SUCCESS(status))goto fail;
    WDF_PDO_EVENT_CALLBACKS_INIT(&callbacks);
    callbacks.EvtDeviceResourceRequirementsQuery=ChildRequirements;
    WdfPdoInitSetEventCallbacks(init,&callbacks);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,A04_CHILD_CONTEXT);
    status=WdfDeviceCreate(&init,&attr,&child);
    if(!NT_SUCCESS(status))goto fail;
    ChildContext(child)->base=base;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,NULL,&A04SharedInterfaceGuid,NULL);
    query.SendQueryToParentStack=TRUE;
    status=WdfDeviceAddQueryInterface(child,&query);
    if(!NT_SUCCESS(status)){WdfObjectDelete(child);return status;}
    status=WdfFdoAddStaticChild(parent,child);
    if(!NT_SUCCESS(status))WdfObjectDelete(child);
    return status;
fail:
    if(init)WdfDeviceInitFree(init);
    return status;
}
#endif
NTSTATUS ParentAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp;WDF_OBJECT_ATTRIBUTES attr;WDFDEVICE dev;NTSTATUS status;
#if defined(A04_SHARED_QUERY_EXPERIMENTAL) && A04_SHARED_QUERY_EXPERIMENTAL
    A04_LEGACY_SHARED_INTERFACE iface;WDF_QUERY_INTERFACE_CONFIG query;
    A04_SHARED_INTERFACE inputIface;
#endif
    UNREFERENCED_PARAMETER(driver);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp);
    pnp.EvtDevicePrepareHardware=ParentPrepare;pnp.EvtDeviceReleaseHardware=ParentRelease;pnp.EvtDeviceD0Entry=ParentD0;
    pnp.EvtDeviceD0Exit=ParentD0Exit;pnp.EvtDeviceSurpriseRemoval=ParentSurprise;
    WdfDeviceInitSetPnpPowerEventCallbacks(init,&pnp);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr,A04_PARENT_CONTEXT);
    attr.ExecutionLevel=WdfExecutionLevelPassive;
    status=WdfDeviceCreate(&init,&attr,&dev);
    if(!NT_SUCCESS(status))return status;
    {
        A04_PARENT_CONTEXT *c=ParentContext(dev);
        ExInitializeFastMutex(&c->pwrapLock);
        c->pmic.lock_context=&c->pwrapLock;c->pmic.lock=ParentLock;c->pmic.unlock=ParentUnlock;
        c->pmic.bus.context=c;c->pmic.bus.read32=ParentPwrapRead;c->pmic.bus.write32=ParentPwrapWrite;
        c->pmic.bus.delay=ParentPwrapDelay;c->pmic.bus.attempts=1000;
        c->regs.context=c;c->regs.lock=ParentRegLock;c->regs.unlock=ParentRegUnlock;
        c->regs.read32=ParentRegRead;c->regs.write32=ParentRegWrite;
    }
#if defined(A04_SHARED_QUERY_EXPERIMENTAL) && A04_SHARED_QUERY_EXPERIMENTAL
    RtlZeroMemory(&iface,sizeof(iface));iface.Header.Size=sizeof(iface);iface.Header.Version=1;
    iface.Header.Context=dev;iface.Header.InterfaceReference=ParentInterfaceReference;
    iface.Header.InterfaceDereference=ParentInterfaceDereference;iface.ReadButtons=ParentReadButtons;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&iface,&GUID_A04_MT8167_SHARED_INTERFACE,NULL);
    status=WdfDeviceAddQueryInterface(dev,&query);
    if(!NT_SUCCESS(status))return status;
    RtlZeroMemory(&inputIface,sizeof(inputIface));
    inputIface.Size=sizeof(inputIface);inputIface.Version=A04_SHARED_INTERFACE_VERSION;
    inputIface.Context=dev;inputIface.InterfaceReference=ParentInterfaceReference;
    inputIface.InterfaceDereference=ParentInterfaceDereference;
    inputIface.TouchUpdate=ParentTouchUpdate;inputIface.TouchWrite=ParentTouchWrite;
    inputIface.TouchPen=ParentTouchPen;inputIface.ButtonPmicRead=ParentButtonRead;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&inputIface,&A04SharedInterfaceGuid,NULL);
    status=WdfDeviceAddQueryInterface(dev,&query);
#endif
#if defined(A04_SHARED_CHILD_PROBE) && A04_SHARED_CHILD_PROBE
    if(!NT_SUCCESS(status))return status;
    status=ParentAddProbeChild(dev,L"A04BR3\\SharedButtonsProbe",L"0",0x10002000u);
    if(!NT_SUCCESS(status))return status;
    status=ParentAddProbeChild(dev,L"A04BR3\\SharedTouchProbe",L"0",0x11009000u);
#endif
    return status;
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry)
{
    WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,ParentAdd);
    return WdfDriverCreate(driver,registry,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
