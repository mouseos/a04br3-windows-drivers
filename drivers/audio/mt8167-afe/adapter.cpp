/* Non-operational PortCls adapter skeleton: no subdevices and no MMIO. */
#include <portcls.h>
#include "wavert_miniport.h"
#include "wavert_registration.h"
#if defined(A04_MT8167_OWNER_DIAGNOSTIC) && A04_MT8167_OWNER_DIAGNOSTIC && defined(A04_MT8167_WAVERT_DIAGNOSTIC) && A04_MT8167_WAVERT_DIAGNOSTIC && defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
#include "pnp_backend.h"
struct Mt8167DeviceExtension { ULONG_PTR portclsReserved[PORT_CLASS_DEVICE_EXTENSION_SIZE / sizeof(ULONG_PTR)]; Mt8167PnpBackend *owner; };
static NTSTATUS Mt8167PnpDispatch(PDEVICE_OBJECT device,PIRP irp)
{
    PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(irp);
    if (stack->MinorFunction==IRP_MN_STOP_DEVICE || stack->MinorFunction==IRP_MN_SURPRISE_REMOVAL || stack->MinorFunction==IRP_MN_REMOVE_DEVICE) {
        Mt8167DeviceExtension *ext=static_cast<Mt8167DeviceExtension *>(device->DeviceExtension);
        Mt8167PnpBackend *owner=static_cast<Mt8167PnpBackend *>(InterlockedExchangePointer(reinterpret_cast<PVOID volatile *>(&ext->owner),NULL));
        if (owner) { Mt8167PnpBackendRevoke(owner);Mt8167PnpBackendRelease(owner); }
    }
    return PcDispatchIrp(device,irp);
}
#endif
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
    NTSTATUS status;
#if defined(A04_MT8167_OWNER_DIAGNOSTIC) && A04_MT8167_OWNER_DIAGNOSTIC
    Mt8167DeviceExtension *ext=static_cast<Mt8167DeviceExtension *>(device->DeviceExtension);
    PDEVICE_OBJECT pdo=NULL;
    Mt8167PnpBackend *owner=NULL;
    if (ext->owner) return STATUS_DEVICE_BUSY;
    status=PcGetPhysicalDeviceObject(device,&pdo);
    if (!NT_SUCCESS(status) || !pdo) return NT_SUCCESS(status)?STATUS_DEVICE_CONFIGURATION_ERROR:status;
    status=Mt8167PnpBackendCreate(pdo,resources,&owner);
    if (!NT_SUCCESS(status)) return status;
    status=Mt8167NewMiniportWithBackend(&miniport,Mt8167PnpBackendInterface(owner));
    if (NT_SUCCESS(status)) { status=Mt8167RegisterWaveRt(device,irp,resources,miniport);miniport->Release(); }
    if (!NT_SUCCESS(status)) { Mt8167PnpBackendRevoke(owner);Mt8167PnpBackendRelease(owner);return status; }
    ext->owner=owner;
    return status;
#else
    status = Mt8167NewMiniport(&miniport);
    if (!NT_SUCCESS(status)) return status;
    status = Mt8167RegisterWaveRt(device, irp, resources, miniport);
    miniport->Release();
    return status;
#endif
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
#if defined(A04_MT8167_OWNER_DIAGNOSTIC) && A04_MT8167_OWNER_DIAGNOSTIC && defined(A04_MT8167_WAVERT_DIAGNOSTIC) && A04_MT8167_WAVERT_DIAGNOSTIC && defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
    return PcAddAdapterDevice(driver,pdo,PCPFNSTARTDEVICE(Mt8167StartDevice),1,sizeof(Mt8167DeviceExtension));
#else
    return PcAddAdapterDevice(driver, pdo, PCPFNSTARTDEVICE(Mt8167StartDevice),1,0);
#endif
}
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    NTSTATUS status=PcInitializeAdapterDriver(driver, path, Mt8167AddDevice);
#if defined(A04_MT8167_OWNER_DIAGNOSTIC) && A04_MT8167_OWNER_DIAGNOSTIC && defined(A04_MT8167_WAVERT_DIAGNOSTIC) && A04_MT8167_WAVERT_DIAGNOSTIC && defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
    if (NT_SUCCESS(status)) driver->MajorFunction[IRP_MJ_PNP]=Mt8167PnpDispatch;
#endif
    return status;
}
