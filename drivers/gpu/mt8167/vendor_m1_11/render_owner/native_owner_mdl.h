/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_NATIVE_OWNER_MDL_H
#define A04_NATIVE_OWNER_MDL_H
#include <ntddk.h>
#include <stdint.h>
struct a04_native_mdl_binding;
/* Miniport-owned VidMm reference. acquire must retain the exact MDL/pages and
 * establish their Normal-RAM allocation origin (not a guessed cache flag).
 * It must fail for Device/common-buffer/user-forged MDLs. release drops only
 * this reference; it must not unlock/free VidMm pages. Context is retained
 * together with the binding until cold release or proven future retirement. */
struct a04_native_mdl_lease {
    void *context;
    NTSTATUS (*acquire)(void *,PMDL);
    void (*release)(void *,PMDL);
};
/* PASSIVE serialized CPU-owned backing lease. Only WDDM1.x physical
 * MAP_APERTURE backing is accepted; bind retains physical pages immediately.
 * sync_for_gpu changes cache ownership, never allocates HAL packet DMA. */
NTSTATUS a04_native_mdl_bind(struct a04_native_mdl_binding **,PDMA_ADAPTER,
    PDEVICE_OBJECT,PMDL,ULONG bytes,const struct a04_native_mdl_lease *);
/* Input and output use the same WDDM page-lifetime contract. */
NTSTATUS a04_native_mdl_bind_input(struct a04_native_mdl_binding **,PDMA_ADAPTER,
 PDEVICE_OBJECT,PMDL,ULONG,const struct a04_native_mdl_lease *);
ULONG a04_native_mdl_extent(const struct a04_native_mdl_binding *);
PDMA_ADAPTER a04_native_mdl_adapter(const struct a04_native_mdl_binding *);
/* A native cold target holds an additional logical reference. Caller must
 * remove that cold target's mappings before dropping it. */
NTSTATUS a04_native_mdl_hold_cold(struct a04_native_mdl_binding *);
NTSTATUS a04_native_mdl_drop_cold(struct a04_native_mdl_binding *);
const uint64_t *a04_native_mdl_pages(const struct a04_native_mdl_binding *,ULONG *count);
void *a04_native_mdl_cpu(const struct a04_native_mdl_binding *);
PMDL a04_native_mdl_original(const struct a04_native_mdl_binding *);
/* Exact locked-MDL CPU clean before GPU publication; invalidate only after
 * real job/SLC/GP completion. Residency PTE/page references survive until
 * WDDM UNMAP + real native retirement. No HAL SG/map-register lifetime. */
NTSTATUS a04_native_mdl_sync_for_gpu(struct a04_native_mdl_binding *);
NTSTATUS a04_native_mdl_sync_for_cpu(struct a04_native_mdl_binding *);
/* Call BEFORE any GPU page-table root can expose these pages. One-way until
 * a future native drain/unmap/IRQ-rundown implementation proves retirement. */
NTSTATUS a04_native_mdl_mark_exposed(struct a04_native_mdl_binding *);
NTSTATUS a04_native_mdl_release_cold(struct a04_native_mdl_binding **);
/* Provider-private retirement step. Caller must have completed the fixed
 * FW off/real IRQ drain/RGXStop/platform-off + callback gate/rundown contract.
 * This only drops the exposure marker; targets/pages remain retained until
 * ordered cold detach. Never call based only on idle, a mask, or CPU poll. */
NTSTATUS a04_native_mdl_retire_after_gpu_off(struct a04_native_mdl_binding *);
/* Identity-only addresses: comparisons/overlap checks, never CPU access or
 * GPU PTE construction. GPU PTEs must use pages() while the WDDM lease remains retained. */
const uint64_t *a04_native_mdl_physical_identity(const struct a04_native_mdl_binding *,ULONG *);
const void *a04_native_mdl_cpu_identity(const struct a04_native_mdl_binding *);
#endif
