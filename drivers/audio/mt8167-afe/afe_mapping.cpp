#include "afe_mapping.h"
NTSTATUS Mt8167MapAssignedAfe(PRESOURCELIST resources,Mt8167AfeMapping *mapping)
{
    PCM_PARTIAL_RESOURCE_DESCRIPTOR afe=NULL,sram=NULL;
    if (!resources || !mapping || mapping->base) return STATUS_INVALID_PARAMETER;
    if (resources->NumberOfEntriesOfType(CmResourceTypeMemory)!=2) return STATUS_DEVICE_CONFIGURATION_ERROR;
    for (ULONG i=0;i<2;i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d=resources->FindTranslatedEntry(CmResourceTypeMemory,i);
        if (!d || d->ShareDisposition!=CmResourceShareDeviceExclusive) return STATUS_DEVICE_CONFIGURATION_ERROR;
        if (d->u.Memory.Start.QuadPart==0x11140000LL && d->u.Memory.Length==0x1000) {
            if (afe) return STATUS_DEVICE_CONFIGURATION_ERROR;
            afe=d;
        } else if (d->u.Memory.Start.QuadPart==0x11141000LL && d->u.Memory.Length==0x9000) {
            if (sram) return STATUS_DEVICE_CONFIGURATION_ERROR;
            sram=d;
        } else return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    if (!afe || !sram) return STATUS_DEVICE_CONFIGURATION_ERROR;
    mapping->base=MmMapIoSpace(afe->u.Memory.Start,0x1000,MmNonCached);
    return mapping->base ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
}
void Mt8167UnmapAssignedAfe(Mt8167AfeMapping *mapping)
{
    if (mapping && mapping->base) { MmUnmapIoSpace(mapping->base,0x1000);mapping->base=NULL; }
}
