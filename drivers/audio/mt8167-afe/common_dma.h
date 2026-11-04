#ifndef MT8167_COMMON_DMA_H
#define MT8167_COMMON_DMA_H
#include <ntddk.h>
/* PASSIVE_LEVEL and external serialization. Parent retains DMA_ADAPTER until
 * this object is released. This helper is not connected to PnP or WaveRT. */
struct Mt8167CommonDma {
    PDMA_ADAPTER adapter;
    PVOID cpu;
    PMDL mdl;
    PHYSICAL_ADDRESS logical;
    ULONG size;
    BOOLEAN prepared;
    BOOLEAN exposed;
};
NTSTATUS Mt8167CommonAllocate(PDMA_ADAPTER adapter, ULONG size, Mt8167CommonDma *out);
/* Caller must stop CPU writes and establish the platform cache contract.
 * This is only a pre-publication flush, not a live circular-stream protocol. */
NTSTATUS Mt8167CommonPrepareForDevice(Mt8167CommonDma *buffer);
/* Call before any MMIO can publish its logical address; exposure is sticky. */
NTSTATUS Mt8167CommonMarkExposed(Mt8167CommonDma *buffer);
/* No release is allowed once exposed: register readback is not a drain proof. */
NTSTATUS Mt8167CommonReleaseUnexposed(Mt8167CommonDma *buffer);
#endif
