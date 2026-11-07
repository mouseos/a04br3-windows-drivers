#include "dma_adapter.h"
static NTSTATUS GetDmaAdapter(PDEVICE_OBJECT physicalDevice,PDMA_ADAPTER *out,PULONG mapRegisters
#if A04_AUDIO_START_PERSIST
 ,Mt8167AudioStartRecord *record
#endif
 )
{
    DEVICE_DESCRIPTION d;
    PDMA_ADAPTER adapter;
    if (!physicalDevice || !out || !mapRegisters) return STATUS_INVALID_PARAMETER;
    *out = NULL; *mapRegisters = 0;
#if A04_AUDIO_START_PERSIST
    if(KeGetCurrentIrql()!=PASSIVE_LEVEL)return STATUS_INVALID_DEVICE_STATE;
#endif
    RtlZeroMemory(&d, sizeof(d));
    /* Version 3 is supported from Windows 8; this is not a claim that v1
     * is unsupported on ARM. Width belongs to DmaAddressWidth for v3. */
    d.Version = DEVICE_DESCRIPTION_VERSION3;
    d.Master = TRUE;
    d.ScatterGather = FALSE;
    d.DmaAddressWidth = 32;
    d.InterfaceType = Internal;
    d.MaximumLength = 1048576;
#if A04_AUDIO_START_PERSIST
    if(record){record->dma_called=1;record->description_version=d.Version;
        record->scatter_gather=d.ScatterGather;record->address_width=d.DmaAddressWidth;}
#endif
    adapter = IoGetDmaAdapter(physicalDevice, &d, mapRegisters);
#if A04_AUDIO_START_PERSIST
    if(record){record->map_registers=*mapRegisters;record->adapter_present=adapter!=NULL;
        record->operations_present=adapter&&adapter->DmaOperations;}
#endif
    /* NULL has no detailed NTSTATUS output. Do not call it proof of pool exhaustion. */
    if (!adapter) return STATUS_INSUFFICIENT_RESOURCES;
    if (!adapter->DmaOperations || !adapter->DmaOperations->PutDmaAdapter ||
        !adapter->DmaOperations->AllocateCommonBuffer || !adapter->DmaOperations->FreeCommonBuffer) {
        if (adapter->DmaOperations && adapter->DmaOperations->PutDmaAdapter)
            adapter->DmaOperations->PutDmaAdapter(adapter);
        return STATUS_NOT_SUPPORTED;
    }
    *out = adapter;
    return STATUS_SUCCESS;
}

NTSTATUS Mt8167GetDmaAdapter(PDEVICE_OBJECT pdo,PDMA_ADAPTER *out,PULONG count)
{return GetDmaAdapter(pdo,out,count
#if A04_AUDIO_START_PERSIST
 ,NULL
#endif
 );}
#if A04_AUDIO_START_PERSIST
NTSTATUS Mt8167GetDmaAdapterWithRecord(PDEVICE_OBJECT pdo,PDMA_ADAPTER *out,PULONG count,Mt8167AudioStartRecord *record)
{return GetDmaAdapter(pdo,out,count,record);}
#endif
