/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "BDD.hxx"
#include "ge8300_display_resource_check.h"
#if A04_GE8300_WDDM_DIRECT_EXPERIMENT
NTSTATUS A04DisplayResources::Start(PDEVICE_OBJECT pdo,const DXGK_DEVICE_INFO *info)
{
    PCM_RESOURCE_LIST list; PCM_PARTIAL_RESOURCE_LIST partial;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR ovl=NULL,irq=NULL;
    ULONG matches=0,interrupts=0,i;DEVICE_DESCRIPTION desc;
    if (!pdo || !info || info->PhysicalDeviceObject!=pdo || Adapter || Ovl ||
        KeGetCurrentIrql()!=PASSIVE_LEVEL) return STATUS_INVALID_PARAMETER;
    list=info->TranslatedResourceList;
    if (!list || list->Count!=1) return STATUS_DEVICE_CONFIGURATION_ERROR;
    partial=&list->List[0].PartialResourceList;
    for (i=0;i<partial->Count;i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR item=&partial->PartialDescriptors[i];
        if (item->Type==CmResourceTypeMemory &&
            (ULONGLONG)item->u.Memory.Start.QuadPart==0x14007000ULL) { ovl=item; matches++; }
        if (item->Type==CmResourceTypeInterrupt) { irq=item; interrupts++; }
    }
    if (!ovl || !a04_display_resources_valid(matches,interrupts,
        ovl->ShareDisposition==CmResourceShareDeviceExclusive,
        irq && irq->ShareDisposition==CmResourceShareDeviceExclusive,
        (uint64_t)ovl->u.Memory.Start.QuadPart,ovl->u.Memory.Length))
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    /* A GPIO or unrelated ACPI PDO must never be substituted for the OVL PDO. */
    RtlZeroMemory(&desc,sizeof(desc));desc.Version=DEVICE_DESCRIPTION_VERSION3;
    desc.Master=TRUE;desc.ScatterGather=TRUE;desc.DmaAddressWidth=32;
    desc.InterfaceType=Internal;desc.MaximumLength=4096000;
    Adapter=IoGetDmaAdapter(pdo,&desc,&MapRegisters);
    if (!Adapter || !Adapter->DmaOperations ||
        Adapter->DmaOperations->Size<
            FIELD_OFFSET(DMA_OPERATIONS,PutScatterGatherList)+sizeof(Adapter->DmaOperations->PutScatterGatherList) ||
        !Adapter->DmaOperations->GetScatterGatherList || !Adapter->DmaOperations->PutScatterGatherList ||
        MapRegisters<1000) { Stop(); return STATUS_NOT_SUPPORTED; }
    Ovl=(volatile ULONG *)MmMapIoSpace(ovl->u.Memory.Start,0x1000,MmNonCached);
    if (!Ovl) { Stop(); return STATUS_INSUFFICIENT_RESOURCES; }
    HasInterrupt=(interrupts==1);
    return STATUS_SUCCESS;
}
VOID A04DisplayResources::Stop()
{
    /* Until an OVL scanout is connected no DMA map, IRQ or GPU lease exists. */
    if (Ovl) { MmUnmapIoSpace((void *)Ovl,0x1000); Ovl=NULL; }
    if (Adapter && Adapter->DmaOperations && Adapter->DmaOperations->PutDmaAdapter)
        Adapter->DmaOperations->PutDmaAdapter(Adapter);
    Adapter=NULL;
    MapRegisters=0;HasInterrupt=FALSE;
}
#endif
