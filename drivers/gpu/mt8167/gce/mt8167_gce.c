/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 GCE PDO resource owner. No clock, IRQ, event or command submission
 * is enabled here. A display-adapter DMA address is never accepted. */
#include <ntddk.h>
#include <wdf.h>
#include "../wddm-display/ge8300_display_resource_check.h"
#ifndef A04_GCE_PARENT_CLOCK_INTERFACE
#define A04_GCE_PARENT_CLOCK_INTERFACE 0
#endif
#if A04_GCE_PARENT_CLOCK_INTERFACE
#include "gce_clock_interface.h"
#endif
#ifndef A04_GCE_DISPLAY_LEASE_INTERFACE
#define A04_GCE_DISPLAY_LEASE_INTERFACE 0
#endif
#if A04_GCE_DISPLAY_LEASE_INTERFACE
#include "gce_display_interface.h"
#endif
#define GCE_BYTES 4096u
typedef struct gce_context {
    PDMA_ADAPTER adapter;
    PDEVICE_OBJECT pdo;
    volatile ULONG *registers;
    PVOID command_cpu;
    PHYSICAL_ADDRESS command_dma;
    ULONG map_registers;
#if A04_GCE_PARENT_CLOCK_INTERFACE
    WDFIOTARGET parent_target;
    A04_GCE_CLOCK_INTERFACE parent_clock;
    ULONG clock_token;NTSTATUS clock_status;
#endif
} GCE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(GCE_CONTEXT,GceContext)
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD GceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE GcePrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE GceRelease;
#if A04_GCE_DISPLAY_LEASE_INTERFACE
static VOID GceInterfaceReference(PVOID p){WdfObjectReference((WDFDEVICE)p);}
static VOID GceInterfaceDereference(PVOID p){WdfObjectDereference((WDFDEVICE)p);}
static NTSTATUS GceDisplayAcquire(PVOID p,ULONG *token)
{
    if(!p || !token)return STATUS_INVALID_PARAMETER;
    *token=0;
    /* No platform-owned clock, GCE thread, IRQ and event exclusivity proof
     * exists. Do not issue a lease or expose DMA/register pointers. */
    return STATUS_DEVICE_NOT_READY;
}
static NTSTATUS GceDisplayRelease(PVOID p,ULONG token)
{ UNREFERENCED_PARAMETER(p);UNREFERENCED_PARAMETER(token);return STATUS_NOT_SUPPORTED; }
#endif
#if A04_GCE_PARENT_CLOCK_INTERFACE
static NTSTATUS GceConnectParent(WDFDEVICE device,struct gce_context *c)
{
    PWSTR links=NULL;UNICODE_STRING path;WDF_IO_TARGET_OPEN_PARAMS open;
    WDF_OBJECT_ATTRIBUTES attributes;NTSTATUS status;size_t length;
    status=IoGetDeviceInterfaces((LPGUID)&A04GceParentDeviceInterfaceGuid,NULL,0,&links);
    if(!NT_SUCCESS(status))return status;
    if(!links || !*links){status=STATUS_NO_SUCH_DEVICE;goto done;}
    RtlInitUnicodeString(&path,links);length=path.Length/sizeof(WCHAR);
    /* No arbitrary selection if multiple parents appear. */
    if(links[length+1]){status=STATUS_DEVICE_CONFIGURATION_ERROR;goto done;}
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);attributes.ParentObject=device;
    status=WdfIoTargetCreate(device,&attributes,&c->parent_target);
    if(!NT_SUCCESS(status))goto done;
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&open,&path,GENERIC_READ|GENERIC_WRITE);
    status=WdfIoTargetOpen(c->parent_target,&open);
    if(!NT_SUCCESS(status))goto fail;
    RtlZeroMemory(&c->parent_clock,sizeof(c->parent_clock));
    status=WdfIoTargetQueryForInterface(c->parent_target,&A04GceClockInterfaceGuid,
        (PINTERFACE)&c->parent_clock,sizeof(c->parent_clock),A04_GCE_CLOCK_INTERFACE_VERSION,NULL);
    if(!NT_SUCCESS(status))goto close;
    if(!c->parent_clock.Header.InterfaceDereference || !c->parent_clock.Acquire || !c->parent_clock.Release) {
        status=STATUS_NOINTERFACE;goto dereference;
    }
    /* The parent currently reports not-ready because firmware ownership of
     * the shared clock has not been proved; no GCE access is enabled. */
    status=c->parent_clock.Acquire(c->parent_clock.Header.Context,&c->clock_token);
    if(c->clock_token)goto done; /* retain even when acquisition returned error */
    if(NT_SUCCESS(status))status=STATUS_DEVICE_CONFIGURATION_ERROR;
dereference:
    if(c->parent_clock.Header.InterfaceDereference)c->parent_clock.Header.InterfaceDereference(c->parent_clock.Header.Context);
    RtlZeroMemory(&c->parent_clock,sizeof(c->parent_clock));
close:
    WdfIoTargetClose(c->parent_target);
fail:
    WdfObjectDelete(c->parent_target);c->parent_target=NULL;
done:
    if(links)ExFreePool(links);return status;
}
#endif
static void GceFree(struct gce_context *c)
{
    if(c->command_cpu && c->adapter && c->adapter->DmaOperations &&
       c->adapter->DmaOperations->FreeCommonBuffer) {
        c->adapter->DmaOperations->FreeCommonBuffer(c->adapter,GCE_BYTES,c->command_dma,c->command_cpu,FALSE);
        c->command_cpu=NULL;
    }
    if(c->adapter && c->adapter->DmaOperations &&
       c->adapter->DmaOperations->Size>=FIELD_OFFSET(DMA_OPERATIONS,PutDmaAdapter)+sizeof(c->adapter->DmaOperations->PutDmaAdapter) &&
       c->adapter->DmaOperations->PutDmaAdapter) {
        c->adapter->DmaOperations->PutDmaAdapter(c->adapter);c->adapter=NULL;
    }
    if(c->registers) {MmUnmapIoSpace((PVOID)c->registers,0x1000);c->registers=NULL;}
    c->pdo=NULL;c->map_registers=0;c->command_dma.QuadPart=0;
}
NTSTATUS GcePrepare(WDFDEVICE device,WDFCMRESLIST raw,WDFCMRESLIST translated)
{
    struct gce_context *c=GceContext(device);PCM_PARTIAL_RESOURCE_DESCRIPTOR mem=NULL,irq=NULL;
    DEVICE_DESCRIPTION desc;ULONG n,i,matches=0,interrupts=0;NTSTATUS status=STATUS_DEVICE_CONFIGURATION_ERROR;
    UNREFERENCED_PARAMETER(raw);
    if(c->adapter || c->registers || c->command_cpu)return STATUS_INVALID_DEVICE_STATE;
    n=WdfCmResourceListGetCount(translated);
    for(i=0;i<n;i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d=WdfCmResourceListGetDescriptor(translated,i);
        if(!d)continue;
        if(d->Type==CmResourceTypeMemory && (ULONGLONG)d->u.Memory.Start.QuadPart==0x1020a000ULL){mem=d;matches++;}
        if(d->Type==CmResourceTypeInterrupt){irq=d;interrupts++;}
    }
    if(!mem || !irq || !a04_gce_resources_valid(matches,interrupts,
        mem->ShareDisposition==CmResourceShareDeviceExclusive,
        irq->ShareDisposition==CmResourceShareDeviceExclusive,
        (uint64_t)mem->u.Memory.Start.QuadPart,mem->u.Memory.Length))return status;
    c->pdo=WdfDeviceWdmGetPhysicalDevice(device);
    if(!c->pdo)return STATUS_INVALID_DEVICE_STATE;
    RtlZeroMemory(&desc,sizeof(desc));desc.Version=DEVICE_DESCRIPTION_VERSION3;
    desc.Master=TRUE;desc.ScatterGather=FALSE;desc.DmaAddressWidth=32;
    desc.InterfaceType=Internal;desc.MaximumLength=GCE_BYTES;
    c->adapter=IoGetDmaAdapter(c->pdo,&desc,&c->map_registers);
    if(!c->adapter || !c->adapter->DmaOperations ||
       c->adapter->DmaOperations->Size<FIELD_OFFSET(DMA_OPERATIONS,FreeCommonBuffer)+sizeof(c->adapter->DmaOperations->FreeCommonBuffer) ||
       !c->adapter->DmaOperations->AllocateCommonBuffer || !c->adapter->DmaOperations->FreeCommonBuffer ||
       !c->adapter->DmaOperations->PutDmaAdapter || !c->map_registers) {status=STATUS_NOT_SUPPORTED;goto fail;}
    c->command_cpu=c->adapter->DmaOperations->AllocateCommonBuffer(c->adapter,GCE_BYTES,&c->command_dma,FALSE);
    if(!c->command_cpu){status=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
    if(!c->command_dma.QuadPart || c->command_dma.QuadPart>0xfffff000LL ||
       (c->command_dma.LowPart&4095u) || ((ULONG_PTR)c->command_cpu&4095u)) {status=STATUS_DEVICE_CONFIGURATION_ERROR;goto fail;}
    c->registers=(volatile ULONG *)MmMapIoSpace(mem->u.Memory.Start,0x1000,MmNonCached);
    if(!c->registers){status=STATUS_INSUFFICIENT_RESOURCES;goto fail;}
    /* No device DMA, MMIO access or interrupt is initiated. */
#if A04_GCE_PARENT_CLOCK_INTERFACE
    c->clock_status=GceConnectParent(device,c);
#endif
    return STATUS_SUCCESS;
fail:
    GceFree(c);return status;
}
NTSTATUS GceRelease(WDFDEVICE device,WDFCMRESLIST translated)
{
    struct gce_context *c=GceContext(device);UNREFERENCED_PARAMETER(translated);
#if A04_GCE_PARENT_CLOCK_INTERFACE
    if(c->clock_token)return STATUS_DEVICE_BUSY;
#endif
    GceFree(c);return STATUS_SUCCESS;
}
NTSTATUS GceAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS callbacks;WDF_OBJECT_ATTRIBUTES attributes;WDFDEVICE device;
#if A04_GCE_DISPLAY_LEASE_INTERFACE
    A04_GCE_DISPLAY_INTERFACE iface;WDF_QUERY_INTERFACE_CONFIG query;NTSTATUS status;
#endif
    UNREFERENCED_PARAMETER(driver);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&callbacks);callbacks.EvtDevicePrepareHardware=GcePrepare;
    callbacks.EvtDeviceReleaseHardware=GceRelease;
    WdfDeviceInitSetPnpPowerEventCallbacks(init,&callbacks);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,GCE_CONTEXT);
#if A04_GCE_DISPLAY_LEASE_INTERFACE
    status=WdfDeviceCreate(&init,&attributes,&device);if(!NT_SUCCESS(status))return status;
    RtlZeroMemory(&iface,sizeof(iface));iface.Header.Size=sizeof(iface);
    iface.Header.Version=A04_GCE_DISPLAY_INTERFACE_VERSION;iface.Header.Context=device;
    iface.Header.InterfaceReference=GceInterfaceReference;iface.Header.InterfaceDereference=GceInterfaceDereference;
    iface.Acquire=GceDisplayAcquire;iface.Release=GceDisplayRelease;
    WDF_QUERY_INTERFACE_CONFIG_INIT(&query,(PINTERFACE)&iface,&A04GceDisplayInterfaceGuid,NULL);
    status=WdfDeviceAddQueryInterface(device,&query);if(!NT_SUCCESS(status))return status;
    return WdfDeviceCreateDeviceInterface(device,&A04GceDeviceInterfaceGuid,NULL);
#else
    return WdfDeviceCreate(&init,&attributes,&device);
#endif
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry)
{
    WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,GceAdd);
    return WdfDriverCreate(driver,registry,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
