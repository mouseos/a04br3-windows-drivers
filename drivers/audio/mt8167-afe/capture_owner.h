/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_CAPTURE_OWNER_H
#define MT8167_CAPTURE_OWNER_H
#include "stream_backend.h"
#include "capture_status.h"
/* Existing PnP owner supplies its single AFE mapping and referenced real
 * ADC/clock provider. AcquireInput must program/verify the actual board codec
 * route, UL sample rate, clock/rail/pins, and exclusive AFE ownership. This is
 * deliberately not the playback AcquireMuted interface. No default provider. */
struct Mt8167CaptureResources {
 PDEVICE_OBJECT pdo;PVOID afe;PDMA_ADAPTER adapter;
 ULONG vector;KIRQL level;KAFFINITY affinity;KINTERRUPT_MODE mode;
 PVOID context;void (*reference)(PVOID);void (*dereference)(PVOID);
 /* Actual FIFO/codec timing, or STATUS_NOT_SUPPORTED (no valid latency export). */
 NTSTATUS (*queryLatency)(PVOID,KSRTAUDIO_HWLATENCY *);
 NTSTATUS (*acquireInput)(PVOID,ULONG);
 NTSTATUS (*releaseUnexposed)(PVOID);
};
struct Mt8167CaptureOwner;
NTSTATUS Mt8167CaptureCreate(const Mt8167CaptureResources *,Mt8167CaptureOwner **);
const Mt8167StreamBackend *Mt8167CaptureBackend(Mt8167CaptureOwner *);
/* Before PnP teardown. Exposed pages and the provider remain pinned because
 * an IRQ rundown/register stop does not certify independent bus drain. */
NTSTATUS Mt8167CaptureSnapshot(Mt8167CaptureOwner *,struct a04_capture_status *);
NTSTATUS Mt8167CaptureRevoke(Mt8167CaptureOwner *);
/* Drops only the caller reference; exposed DMA retains its owner until reset. */
void Mt8167CaptureRelease(Mt8167CaptureOwner *);
#endif
