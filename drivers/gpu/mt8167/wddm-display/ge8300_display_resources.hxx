/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_DISPLAY_RESOURCES_HXX
#define A04_DISPLAY_RESOURCES_HXX
#ifndef A04_GE8300_WDDM_DIRECT_EXPERIMENT
#define A04_GE8300_WDDM_DIRECT_EXPERIMENT 0
#endif
#ifndef A04_GE8300_WDDM_SHARED_DIAGNOSTIC
#define A04_GE8300_WDDM_SHARED_DIAGNOSTIC 0
#endif
#ifndef A04_GE8300_WDDM_SHARED_AUTOSTART
#define A04_GE8300_WDDM_SHARED_AUTOSTART 0
#endif
#if A04_GE8300_WDDM_SHARED_AUTOSTART && !A04_GE8300_WDDM_SHARED_DIAGNOSTIC
#error Shared autostart requires shared diagnostic
#endif
#ifndef A04_GE8300_WDDM_SHARED_LEASE
#define A04_GE8300_WDDM_SHARED_LEASE 0
#endif
#if A04_GE8300_WDDM_SHARED_DIAGNOSTIC && !A04_GE8300_WDDM_SHARED_LEASE
#error Shared diagnostic requires WDDM shared lease
#endif
#if A04_GE8300_WDDM_SHARED_LEASE && !A04_GE8300_WDDM_DIRECT_EXPERIMENT
#error Shared lease requires WDDM direct experiment
#endif
#if A04_GE8300_WDDM_DIRECT_EXPERIMENT
#include "../ge8300_status.h"
#include "../vendor_m1_11/windows_shared_scanout.h"
#include "../vendor_m1_11/windows_shared_ovl.h"
struct A04DisplayResources {
    PDMA_ADAPTER Adapter;
    volatile ULONG *Ovl;
    ULONG MapRegisters;
    BOOLEAN HasInterrupt;
    PDEVICE_OBJECT DisplayPdo;
    struct a04_shared_scanout Shared;
    struct a04_shared_ovl SharedOvl;
#if A04_GE8300_WDDM_SHARED_LEASE
    HANDLE GpuHandle;
    struct a04_ge8300_shared_lease Lease;
    NTSTATUS PrepareViaGpu();
    /* No call site until platform visibility and frame callbacks exist. */
    NTSTATUS SwitchShared(uint32_t expected,a04_shared_ovl_ready ready,
        a04_shared_ovl_boundary boundary,void *context);
#endif
    /* Acquires only. No GPU memory is shared and no IRQ/MMIO writes occur. */
    NTSTATUS Start(PDEVICE_OBJECT pdo, const DXGK_DEVICE_INFO *info);
    /* Only when the GPU owner holds its PDO/adapter lease through Stop. */
    NTSTATUS PrepareShared(PDMA_ADAPTER gpu, PDEVICE_OBJECT gpuPdo);
    VOID Stop();
};
#endif
#endif
