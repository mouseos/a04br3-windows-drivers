#include "dma_adapter.h"
NTSTATUS Mt8167GetDmaAdapter(PDEVICE_OBJECT physicalDevice, PDMA_ADAPTER *out,
                            PULONG mapRegisters)
{
    DEVICE_DESCRIPTION d;
    PDMA_ADAPTER adapter;
    if (!physicalDevice || !out || !mapRegisters) return STATUS_INVALID_PARAMETER;
    *out = NULL; *mapRegisters = 0;
    RtlZeroMemory(&d, sizeof(d));
    /* Same contiguous ARM contract used by the real-started Engine1006.
     * Do not copy the GPU scatter/gather mode or assert v1 is unsupported. */
    d.Version = DEVICE_DESCRIPTION_VERSION3;
    d.Master = TRUE;
    d.ScatterGather = FALSE;
    d.DmaAddressWidth = 32;
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
