/* MT8167 Engine adapter with opt-in analog codec capture. */
#include <portcls.h>
#include "wavert_miniport.h"
#include "wavert_registration.h"
#include "start_record.h"
#if defined(A04_MT8167_OWNER_DIAGNOSTIC) && A04_MT8167_OWNER_DIAGNOSTIC && defined(A04_MT8167_WAVERT_DIAGNOSTIC) && A04_MT8167_WAVERT_DIAGNOSTIC && defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
#include "pnp_backend.h"
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
#include "capture_owner.h"
extern "C" NTSTATUS Mt8167LatencyPropertyGate(PIRP,BOOLEAN *);
#endif
struct Mt8167DeviceExtension {
    ULONG_PTR portclsReserved[PORT_CLASS_DEVICE_EXTENSION_SIZE / sizeof(ULONG_PTR)]; Mt8167PnpBackend *owner;
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
    Mt8167CaptureOwner *capture;
#endif
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
    EX_RUNDOWN_REF diagRundown;volatile LONG diagInitialized,diagReady;
#endif
};
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
static NTSTATUS Mt8167ControlDispatch(PDEVICE_OBJECT device,PIRP irp) {
    PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(irp);
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
    {BOOLEAN handled=FALSE;NTSTATUS gated=Mt8167LatencyPropertyGate(irp,&handled);
     if(handled){irp->IoStatus.Status=gated;irp->IoStatus.Information=0;IoCompleteRequest(irp,IO_NO_INCREMENT);return gated;}}
#endif
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
    if(stack->Parameters.DeviceIoControl.IoControlCode==A04_CAPTURE_READ_STATUS){
     Mt8167DeviceExtension*ext=static_cast<Mt8167DeviceExtension*>(device->DeviceExtension);
     NTSTATUS s=STATUS_DEVICE_NOT_READY;ULONG_PTR bytes=0;
     if(stack->Parameters.DeviceIoControl.InputBufferLength||!irp->AssociatedIrp.SystemBuffer||
        stack->Parameters.DeviceIoControl.OutputBufferLength<sizeof(a04_capture_status))s=STATUS_INVALID_PARAMETER;
     else if(InterlockedCompareExchange(&ext->diagReady,0,0)&&ExAcquireRundownProtection(&ext->diagRundown)){
      if(ext->capture)s=Mt8167CaptureSnapshot(ext->capture,static_cast<a04_capture_status*>(irp->AssociatedIrp.SystemBuffer));
      if(NT_SUCCESS(s))bytes=sizeof(a04_capture_status);ExReleaseRundownProtection(&ext->diagRundown);
     }
     irp->IoStatus.Status=s;irp->IoStatus.Information=bytes;IoCompleteRequest(irp,IO_NO_INCREMENT);return s;
    }
#endif
    if(stack->Parameters.DeviceIoControl.IoControlCode!=A04_AUDIO_READ_STATUS)return PcDispatchIrp(device,irp);
    Mt8167DeviceExtension *ext=static_cast<Mt8167DeviceExtension *>(device->DeviceExtension);
    NTSTATUS status=STATUS_DEVICE_NOT_READY;ULONG_PTR bytes=0;
    if(stack->Parameters.DeviceIoControl.InputBufferLength ||
       stack->Parameters.DeviceIoControl.OutputBufferLength<sizeof(a04_audio_snapshot) || !irp->AssociatedIrp.SystemBuffer)
        status=STATUS_INVALID_PARAMETER;
    else if(InterlockedCompareExchange(&ext->diagReady,0,0) && ExAcquireRundownProtection(&ext->diagRundown)) {
        /* PnP revokes admission and waits for this read before dropping owner. */
        if(ext->owner)status=Mt8167PnpBackendSnapshot(ext->owner,static_cast<a04_audio_snapshot *>(irp->AssociatedIrp.SystemBuffer));
        if(NT_SUCCESS(status))bytes=sizeof(a04_audio_snapshot);
        ExReleaseRundownProtection(&ext->diagRundown);
    }
    irp->IoStatus.Status=status;irp->IoStatus.Information=bytes;IoCompleteRequest(irp,IO_NO_INCREMENT);return status;
}
#endif
static NTSTATUS Mt8167PnpDispatch(PDEVICE_OBJECT device,PIRP irp)
{
    PIO_STACK_LOCATION stack=IoGetCurrentIrpStackLocation(irp);
    if (stack->MinorFunction==IRP_MN_STOP_DEVICE || stack->MinorFunction==IRP_MN_SURPRISE_REMOVAL || stack->MinorFunction==IRP_MN_REMOVE_DEVICE) {
        Mt8167DeviceExtension *ext=static_cast<Mt8167DeviceExtension *>(device->DeviceExtension);
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
        InterlockedExchange(&ext->diagReady,0);
        if(InterlockedCompareExchange(&ext->diagInitialized,0,0))ExWaitForRundownProtectionRelease(&ext->diagRundown);
#endif
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
        if(ext->capture){NTSTATUS s=Mt8167CaptureRevoke(ext->capture);if(!NT_SUCCESS(s)){
          irp->IoStatus.Status=s;irp->IoStatus.Information=0;IoCompleteRequest(irp,IO_NO_INCREMENT);return s;}
          Mt8167CaptureRelease(ext->capture);ext->capture=NULL;}
#endif
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
    PCM_PARTIAL_RESOURCE_DESCRIPTOR a=resources->FindUntranslatedEntry(CmResourceTypeMemory,0);
    PCM_PARTIAL_RESOURCE_DESCRIPTOR b=resources->FindUntranslatedEntry(CmResourceTypeMemory,1);
    PCM_PARTIAL_RESOURCE_DESCRIPTOR ta=resources->FindTranslatedEntry(CmResourceTypeMemory,0);
    PCM_PARTIAL_RESOURCE_DESCRIPTOR tb=resources->FindTranslatedEntry(CmResourceTypeMemory,1);
    PCM_PARTIAL_RESOURCE_DESCRIPTOR irq=resources->FindTranslatedEntry(CmResourceTypeInterrupt,0);
    if (!a || !b || !ta || !tb || !irq || a->ShareDisposition!=CmResourceShareDeviceExclusive ||
        b->ShareDisposition!=CmResourceShareDeviceExclusive || ta->ShareDisposition!=CmResourceShareDeviceExclusive ||
        tb->ShareDisposition!=CmResourceShareDeviceExclusive || a->u.Memory.Length!=ta->u.Memory.Length ||
        b->u.Memory.Length!=tb->u.Memory.Length || a->u.Memory.Start.QuadPart<0 || b->u.Memory.Start.QuadPart<0 ||
        ta->u.Memory.Start.QuadPart<0 || tb->u.Memory.Start.QuadPart<0 ||
        !mt8167_audio_translated_memory((uint64_t)ta->u.Memory.Start.QuadPart,ta->u.Memory.Length,
        (uint64_t)tb->u.Memory.Start.QuadPart,tb->u.Memory.Length)) return FALSE;
    return mt8167_audio_resources_match(3,2,1,(uint64_t)a->u.Memory.Start.QuadPart,
        a->u.Memory.Length,(uint64_t)b->u.Memory.Start.QuadPart,b->u.Memory.Length,
        !(irq->Flags & CM_RESOURCE_INTERRUPT_LATCHED)) ? TRUE : FALSE;
}
#endif
static NTSTATUS StartDeviceBody(PDEVICE_OBJECT device,PIRP irp,PRESOURCELIST resources
#if A04_AUDIO_START_PERSIST
 ,Mt8167AudioStartRecord *record
#endif
 )
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
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
    /* The retained bring-up driver deliberately refuses a second lifecycle. */
    if(InterlockedCompareExchange(&ext->diagInitialized,0,0))return STATUS_DEVICE_BUSY;
    ExInitializeRundownProtection(&ext->diagRundown);InterlockedExchange(&ext->diagInitialized,1);
#endif
#if A04_AUDIO_START_PERSIST
    record->stage=A04_START_PDO;
#endif
    status=PcGetPhysicalDeviceObject(device,&pdo);
    if (!NT_SUCCESS(status) || !pdo) return NT_SUCCESS(status)?STATUS_DEVICE_CONFIGURATION_ERROR:status;
#if A04_AUDIO_START_PERSIST
    status=Mt8167PnpBackendCreateWithRecord(pdo,resources,&owner,record);
#else
    status=Mt8167PnpBackendCreate(pdo,resources,&owner);
#endif
    if (!NT_SUCCESS(status)) return status;
#if A04_AUDIO_START_PERSIST
    record->stage=A04_START_MINIPORT;
#endif
    status=Mt8167NewMiniportWithBackend(&miniport,Mt8167PnpBackendInterface(owner));
    if (NT_SUCCESS(status)) {
#if A04_AUDIO_START_PERSIST
        record->stage=A04_START_REGISTER;
#endif
        status=Mt8167RegisterWaveRt(device,irp,resources,miniport);miniport->Release(); }
    if (!NT_SUCCESS(status)) { Mt8167PnpBackendRevoke(owner);Mt8167PnpBackendRelease(owner);return status; }
    ext->owner=owner;
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
    status=Mt8167PnpBackendCreateCapture(owner,&ext->capture);
    if(NT_SUCCESS(status))status=Mt8167NewMiniportWithBackend(&miniport,Mt8167CaptureBackend(ext->capture));
    if(NT_SUCCESS(status)){status=Mt8167RegisterCaptureWaveRt(device,irp,resources,miniport);miniport->Release();}
    if(!NT_SUCCESS(status))return status;
#endif
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
    InterlockedExchange(&ext->diagReady,1);
#endif
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
static NTSTATUS Mt8167StartDevice(PDEVICE_OBJECT device,PIRP irp,PRESOURCELIST resources)
{
#if A04_AUDIO_START_PERSIST
 Mt8167AudioStartRecord record;RtlZeroMemory(&record,sizeof(record));
 LARGE_INTEGER time;KeQuerySystemTime(&time);record.system_time_low=time.LowPart;record.system_time_high=(ULONG)time.HighPart;
 record.schema=1;record.bytes=sizeof(record);record.inf_revision=
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
 1026;
#else
 1006;
#endif
 record.stage=A04_START_RESOURCES;
 NTSTATUS result=StartDeviceBody(device,irp,resources,&record);record.result=result;
 if(NT_SUCCESS(result))record.stage=A04_START_READY;
 NTSTATUS saved=Mt8167PersistStartRecord(&record);
 DbgPrintEx(DPFLTR_IHVDRIVER_ID,DPFLTR_INFO_LEVEL,"A04AUD START stage=%lu status=%08lx dma=%lu adapter=%lu maps=%lu snapshot=%08lx\n",record.stage,(ULONG)result,record.dma_called,record.adapter_present,record.map_registers,(ULONG)saved);
 return result;
#else
 return StartDeviceBody(device,irp,resources);
#endif
}

NTSTATUS Mt8167AddDevice(PDRIVER_OBJECT driver, PDEVICE_OBJECT pdo)
{
#if defined(A04_MT8167_OWNER_DIAGNOSTIC) && A04_MT8167_OWNER_DIAGNOSTIC && defined(A04_MT8167_WAVERT_DIAGNOSTIC) && A04_MT8167_WAVERT_DIAGNOSTIC && defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
    return PcAddAdapterDevice(driver,pdo,PCPFNSTARTDEVICE(Mt8167StartDevice),
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
        4, /* Existing render pair plus actual capture pair; HFP profile separate. */
#elif defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
        2,
#else
        1,
#endif
        sizeof(Mt8167DeviceExtension));
#else
    return PcAddAdapterDevice(driver, pdo, PCPFNSTARTDEVICE(Mt8167StartDevice),
#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
        4, /* Existing render pair plus actual capture pair; HFP profile separate. */
#elif defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
        2,
#else
        1,
#endif
        0);
#endif
}
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING path)
{
    NTSTATUS status=PcInitializeAdapterDriver(driver, path, Mt8167AddDevice);
#if defined(A04_MT8167_OWNER_DIAGNOSTIC) && A04_MT8167_OWNER_DIAGNOSTIC && defined(A04_MT8167_WAVERT_DIAGNOSTIC) && A04_MT8167_WAVERT_DIAGNOSTIC && defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
    if (NT_SUCCESS(status)) {
        driver->MajorFunction[IRP_MJ_PNP]=Mt8167PnpDispatch;
#if (defined(A04_AUDIO_FIRST_PLAYBACK) && A04_AUDIO_FIRST_PLAYBACK) || (defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM)
        driver->MajorFunction[IRP_MJ_DEVICE_CONTROL]=Mt8167ControlDispatch;
#endif
    }
#endif
    return status;
}
