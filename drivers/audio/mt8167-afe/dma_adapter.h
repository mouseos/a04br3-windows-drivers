#ifndef MT8167_AUDIO_DMA_ADAPTER_H
#define MT8167_AUDIO_DMA_ADAPTER_H
#include <ntddk.h>
#include "start_record.h"
/* Only called by the exclusive owning PnP device at PASSIVE_LEVEL. The caller
 * must not PutDmaAdapter while any common buffer or mapping exists. */
NTSTATUS Mt8167GetDmaAdapter(PDEVICE_OBJECT physicalDevice, PDMA_ADAPTER *out,
                            PULONG mapRegisters);
#if A04_AUDIO_START_PERSIST
NTSTATUS Mt8167GetDmaAdapterWithRecord(PDEVICE_OBJECT,PDMA_ADAPTER *,PULONG,Mt8167AudioStartRecord *);
#endif
#endif
