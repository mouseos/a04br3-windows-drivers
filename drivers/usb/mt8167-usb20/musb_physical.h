/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_MUSB_PHYSICAL_H
#define MT8167_MUSB_PHYSICAL_H
#include "usbport81.h"
#include "musb_async.h"
#include "musb_lanes.h"
#include "root_hub.h"
struct mt_musb_runtime;
/* Open/Close perform parent calls/MMIO only at PASSIVE. Other methods <=
 * DISPATCH. No worker retains hcd/endpoint/transfer/SG/parameters pointers. */
NTSTATUS MtMusbOpen(PDEVICE_OBJECT,struct mt_musb_runtime **);
/* Detach before calling Close. At elevated IRQL cleanup is queued to PASSIVE.
 * FALSE means stop has NOT been proved; it never means successful teardown. */
BOOLEAN MtMusbClose(struct mt_musb_runtime *);
ULONG MtMusbSubmit(struct mt_musb_runtime *,ULONG,const struct a04_mp_properties *,ULONG,
 const struct a04_mp_parameters *,PVOID);
ULONG MtMusbSubmitDeadline(struct mt_musb_runtime *,ULONG,const struct a04_mp_properties *,ULONG,
 const struct a04_mp_parameters *,PVOID,ULONGLONG);
VOID MtMusbCancel(struct mt_musb_runtime *,ULONG);
ULONG MtMusbTake(struct mt_musb_runtime *,ULONG,PVOID,ULONG,PULONG,PULONG,PULONG);
ULONG MtMusbRecoverToggle(struct mt_musb_runtime *,ULONG,PULONG);
VOID MtMusbRetire(struct mt_musb_runtime *,ULONG);
ULONG MtMusbRootQueue(struct mt_musb_runtime *,ULONG);
ULONG MtMusbObserve(struct mt_musb_runtime *,struct a04_hub_observation *,PULONG,PULONG,PULONG);
ULONG MtMusbFrame(struct mt_musb_runtime *);
#endif
