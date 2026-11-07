/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_HFP_PROVIDER_H
#define MT8167_HFP_PROVIDER_H
#include <portcls.h>
#include "../../bus/mt8167-shared/hfp_interface.h"
struct Mt8167HfpOwner;
/* Uses the existing owner's AFE mapping and DMA adapter; never maps MMIO twice.
 * reference/release pins that existing backend until all HFP clients retire. */
struct Mt8167HfpResources {
 PDEVICE_OBJECT fdo,pdo;PRESOURCELIST resources;PVOID afe;
 PDMA_ADAPTER adapter;ULONG vector;KIRQL level;KAFFINITY affinity;
 PVOID owner;void (*reference)(PVOID);void (*release)(PVOID);
};
NTSTATUS Mt8167HfpStart(const Mt8167HfpResources *,Mt8167HfpOwner **);
/* PASSIVE, before original backend revoke, D0 teardown or resource release.
 * Failure retains *owner/FDO/module/mappings. Never retries automatically. */
BOOLEAN Mt8167HfpCanSuspend(Mt8167HfpOwner *);
NTSTATUS Mt8167HfpStop(Mt8167HfpOwner *);
#endif
