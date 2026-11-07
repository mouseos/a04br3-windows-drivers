/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_WAVE_DMA_PROBE_H
#define MT8167_WAVE_DMA_PROBE_H
#include <ntddk.h>
/* PASSIVE_LEVEL, before an MDL is exposed to client or hardware. Probes the
 * DMA adapter mapping and immediately returns it without programming AFE.
 * On STATUS_IO_TIMEOUT the caller must retain adapter, PDO and MDL indefinitely:
 * the adapter callback may still arrive. This does not prove cyclic coherence
 * or hardware DMA drain and does not return a usable live logical address. */
NTSTATUS Mt8167ProbeWaveDma(PDMA_ADAPTER,PDEVICE_OBJECT,PMDL,ULONG);
#endif
