#include "dma_adapter.h"
NTSTATUS Mt8167GetDmaAdapter(PDEVICE_OBJECT physicalDevice, PDMA_ADAPTER *out,
                            PULONG mapRegisters)
{
    DEVICE_DESCRIPTION d;
    PDMA_ADAPTER adapter;
    if (!physicalDevice || !out || !mapRegisters) return STATUS_INVALID_PARAMETER;
    *out = NULL; *mapRegisters = 0;
    RtlZeroMemory(&d, sizeof(d));
    d.Version = DEVICE_DESCRIPTION_VERSION;
    d.Master = TRUE;
    d.ScatterGather = FALSE;
    d.Dma32BitAddresses = TRUE;
    d.InterfaceType = Internal;
    d.MaximumLength = 1048576;
    adapter = IoGetDmaAdapter(physicalDevice, &d, mapRegisters);
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
