#include "afe_mapping.h"
#include "resource_check.h"
NTSTATUS Mt8167MapAssignedAfe(PRESOURCELIST resources,Mt8167AfeMapping *mapping)
{
    PCM_PARTIAL_RESOURCE_DESCRIPTOR afe=NULL,sram=NULL,translated[2],raw[2];
    if (!resources || !mapping || mapping->base) return STATUS_INVALID_PARAMETER;
    if (resources->NumberOfEntriesOfType(CmResourceTypeMemory)!=2) return STATUS_DEVICE_CONFIGURATION_ERROR;
    for (ULONG i=0;i<2;i++) {
        PCM_PARTIAL_RESOURCE_DESCRIPTOR d=resources->FindUntranslatedEntry(CmResourceTypeMemory,i);
        PCM_PARTIAL_RESOURCE_DESCRIPTOR t=resources->FindTranslatedEntry(CmResourceTypeMemory,i);
        if (!d || !t || d->ShareDisposition!=CmResourceShareDeviceExclusive ||
            t->ShareDisposition!=CmResourceShareDeviceExclusive || d->u.Memory.Length!=t->u.Memory.Length ||
            d->u.Memory.Start.QuadPart<0 || t->u.Memory.Start.QuadPart<0) return STATUS_DEVICE_CONFIGURATION_ERROR;
        raw[i]=d;translated[i]=t;
        if (d->u.Memory.Start.QuadPart==0x11140000LL && d->u.Memory.Length==0x1000) {
            if (afe) return STATUS_DEVICE_CONFIGURATION_ERROR;
            afe=t;
        } else if (d->u.Memory.Start.QuadPart==0x11141000LL && d->u.Memory.Length==0x9000) {
            if (sram) return STATUS_DEVICE_CONFIGURATION_ERROR;
            sram=t;
        } else return STATUS_DEVICE_CONFIGURATION_ERROR;
    }
    if (!afe || !sram || !mt8167_audio_resources_match(3,2,1,(uint64_t)raw[0]->u.Memory.Start.QuadPart,
        raw[0]->u.Memory.Length,(uint64_t)raw[1]->u.Memory.Start.QuadPart,raw[1]->u.Memory.Length,1) ||
        !mt8167_audio_translated_memory((uint64_t)translated[0]->u.Memory.Start.QuadPart,translated[0]->u.Memory.Length,
        (uint64_t)translated[1]->u.Memory.Start.QuadPart,translated[1]->u.Memory.Length)) return STATUS_DEVICE_CONFIGURATION_ERROR;
    mapping->base=MmMapIoSpace(afe->u.Memory.Start,0x1000,MmNonCached);
    return mapping->base ? STATUS_SUCCESS : STATUS_INSUFFICIENT_RESOURCES;
}
void Mt8167UnmapAssignedAfe(Mt8167AfeMapping *mapping)
{
    if (mapping && mapping->base) { MmUnmapIoSpace(mapping->base,0x1000);mapping->base=NULL; }
}
