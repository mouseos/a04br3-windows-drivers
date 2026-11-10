/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_WAVE_DMA_LIVE_H
#define MT8167_WAVE_DMA_LIVE_H
#include <ntddk.h>
/* Caller owns adapter, device, MDL and serializes at PASSIVE_LEVEL. Map before
 * client access; timeout retains all inputs until a possible late callback.
 * A successful map remains held until Release. Exposed mappings may be
 * released only after an independent hardware/bus drain proof. */
struct Mt8167WaveDma { PDMA_ADAPTER adapter; PSCATTER_GATHER_LIST list; PHYSICAL_ADDRESS logical; ULONG bytes; BOOLEAN exposed,writeToDevice; };
NTSTATUS Mt8167WaveDmaMap(PDMA_ADAPTER,PDEVICE_OBJECT,PMDL,ULONG,Mt8167WaveDma *);
/* Device-to-memory mapping. The matching PutScatterGatherList also uses FALSE. */
NTSTATUS Mt8167WaveDmaMapCapture(PDMA_ADAPTER,PDEVICE_OBJECT,PMDL,ULONG,Mt8167WaveDma *);
NTSTATUS Mt8167WaveDmaRelease(Mt8167WaveDma *,BOOLEAN drainVerified);
/* CT3 STOP/hw_free protocol: only after synchronized memif disable,
 * IRQ mask/ack + disconnect/rundown and matched parent's stop receipt. */
NTSTATUS Mt8167WaveDmaReleaseStopped(Mt8167WaveDma *);
#endif
