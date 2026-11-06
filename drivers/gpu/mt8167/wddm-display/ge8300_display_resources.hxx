/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_DISPLAY_RESOURCES_HXX
#define A04_DISPLAY_RESOURCES_HXX
#ifndef A04_GE8300_WDDM_DIRECT_EXPERIMENT
#define A04_GE8300_WDDM_DIRECT_EXPERIMENT 0
#endif
#if A04_GE8300_WDDM_DIRECT_EXPERIMENT
struct A04DisplayResources {
    PDMA_ADAPTER Adapter;
    volatile ULONG *Ovl;
    ULONG MapRegisters;
    BOOLEAN HasInterrupt;
    /* Acquires only. No GPU memory is shared and no IRQ/MMIO writes occur. */
    NTSTATUS Start(PDEVICE_OBJECT pdo, const DXGK_DEVICE_INFO *info);
    VOID Stop();
};
#endif
#endif
