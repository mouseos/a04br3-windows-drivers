/* Non-operational PortCls adapter skeleton: no subdevices and no MMIO. */
#include <portcls.h>
#include "wavert_miniport.h"
#include "wavert_registration.h"
#if defined(A04_MT8167_WAVERT_DIAGNOSTIC) && A04_MT8167_WAVERT_DIAGNOSTIC && defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
#include "resource_check.h"
#endif
extern "C" DRIVER_INITIALIZE DriverEntry;
DRIVER_ADD_DEVICE Mt8167AddDevice;
#if defined(A04_MT8167_WAVERT_DIAGNOSTIC) && A04_MT8167_WAVERT_DIAGNOSTIC && defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
static BOOLEAN Mt8167ResourcesValid(PRESOURCELIST resources)
{
    if (!resources || resources->NumberOfEntries()!=3 ||
        resources->NumberOfEntriesOfType(CmResourceTypeMemory)!=2 ||
        resources->NumberOfEntriesOfType(CmResourceTypeInterrupt)!=1) return FALSE;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR a=resources->FindTranslatedEntry(CmResourceTypeMemory,0);
    PCM_PARTIAL_RESOURCE_DESCRIPTOR b=resources->FindTranslatedEntry(CmResourceTypeMemory,1);
    PCM_PARTIAL_RESOURCE_DESCRIPTOR irq=resources->FindTranslatedEntry(CmResourceTypeInterrupt,0);
    if (!a || !b || !irq) return FALSE;
    return mt8167_audio_resources_match(3,2,1,(uint64_t)a->u.Memory.Start.QuadPart,
        a->u.Memory.Length,(uint64_t)b->u.Memory.Start.QuadPart,b->u.Memory.Length,
        !(irq->Flags & CM_RESOURCE_INTERRUPT_LATCHED)) ? TRUE : FALSE;
}
#endif
static NTSTATUS Mt8167StartDevice(PDEVICE_OBJECT device, PIRP irp,
                                   PRESOURCELIST resources)
{
#if defined(A04_MT8167_WAVERT_DIAGNOSTIC) && A04_MT8167_WAVERT_DIAGNOSTIC && defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
    if (!Mt8167ResourcesValid(resources)) return STATUS_DEVICE_CONFIGURATION_ERROR;
    PMINIPORTWAVERT miniport = NULL;
    NTSTATUS status = Mt8167NewMiniport(&miniport);
    if (!NT_SUCCESS(status)) return status;
    status = Mt8167RegisterWaveRt(device, irp, resources, miniport);
    miniport->Release();
    return status;
#else
    UNREFERENCED_PARAMETER(device);
    UNREFERENCED_PARAMETER(irp);
    UNREFERENCED_PARAMETER(resources);
    /* No ownership broker, power/codec path or DMA teardown exists yet. */
    return STATUS_DEVICE_NOT_READY;
#endif
}
NTSTATUS Mt8167AddDevice(PDRIVER_OBJECT driver, PDEVICE_OBJECT pdo)
{
    return PcAddAdapterDevice(driver, pdo, PCPFNSTARTDEVICE(Mt8167StartDevice),
                              1, 0);
}
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    return PcInitializeAdapterDriver(driver, path, Mt8167AddDevice);
}
