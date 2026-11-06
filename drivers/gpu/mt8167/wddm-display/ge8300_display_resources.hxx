/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_DISPLAY_RESOURCES_HXX
#define A04_DISPLAY_RESOURCES_HXX
#ifndef A04_GE8300_WDDM_DIRECT_EXPERIMENT
#define A04_GE8300_WDDM_DIRECT_EXPERIMENT 0
#endif
#if A04_GE8300_WDDM_DIRECT_EXPERIMENT
#include "../vendor_m1_11/windows_shared_scanout.h"
struct A04DisplayResources {
    PDMA_ADAPTER Adapter;
    volatile ULONG *Ovl;
    ULONG MapRegisters;
    BOOLEAN HasInterrupt;
    PDEVICE_OBJECT DisplayPdo;
    struct a04_shared_scanout Shared;
    /* Acquires only. No GPU memory is shared and no IRQ/MMIO writes occur. */
    NTSTATUS Start(PDEVICE_OBJECT pdo, const DXGK_DEVICE_INFO *info);
    /* Only when the GPU owner holds its PDO/adapter lease through Stop. */
    NTSTATUS PrepareShared(PDMA_ADAPTER gpu, PDEVICE_OBJECT gpuPdo);
    VOID Stop();
};
#endif
#endif
