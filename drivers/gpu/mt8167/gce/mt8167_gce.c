/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * MT8167 GCE PDO resource owner. No clock, IRQ, event or command submission
 * is enabled here. A display-adapter DMA address is never accepted. */
#include <ntddk.h>
#include <wdf.h>
#include "../wddm-display/ge8300_display_resource_check.h"
#define GCE_BYTES 4096u
typedef struct gce_context {
    PDMA_ADAPTER adapter;
    PDEVICE_OBJECT pdo;
    volatile ULONG *registers;
    PVOID command_cpu;
    PHYSICAL_ADDRESS command_dma;
    ULONG map_registers;
} GCE_CONTEXT;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(GCE_CONTEXT,GceContext)
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD GceAdd;
EVT_WDF_DEVICE_PREPARE_HARDWARE GcePrepare;
EVT_WDF_DEVICE_RELEASE_HARDWARE GceRelease;
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
    return STATUS_SUCCESS;
fail:
    GceFree(c);return status;
}
NTSTATUS GceRelease(WDFDEVICE device,WDFCMRESLIST translated)
{
    UNREFERENCED_PARAMETER(translated);GceFree(GceContext(device));return STATUS_SUCCESS;
}
NTSTATUS GceAdd(WDFDRIVER driver,PWDFDEVICE_INIT init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS callbacks;WDF_OBJECT_ATTRIBUTES attributes;WDFDEVICE device;
    UNREFERENCED_PARAMETER(driver);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&callbacks);callbacks.EvtDevicePrepareHardware=GcePrepare;
    callbacks.EvtDeviceReleaseHardware=GceRelease;
    WdfDeviceInitSetPnpPowerEventCallbacks(init,&callbacks);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes,GCE_CONTEXT);
    return WdfDeviceCreate(&init,&attributes,&device);
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver,PUNICODE_STRING registry)
{
    WDF_DRIVER_CONFIG config;WDF_DRIVER_CONFIG_INIT(&config,GceAdd);
    return WdfDriverCreate(driver,registry,WDF_NO_OBJECT_ATTRIBUTES,&config,WDF_NO_HANDLE);
}
