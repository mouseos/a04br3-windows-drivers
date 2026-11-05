/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Native m1.11 startup transport adapted from xnu_kernel_benesse_a04br3
 * A04BR3RogueRuntime.h and A04BR3RogueStartupExecutor.h, f5d43bf1677955e7e367545b9bbbd6449776cdda.
 */
#ifndef A04_VENDOR_M1_11_WINDOWS_START_H
#define A04_VENDOR_M1_11_WINDOWS_START_H
#include "windows_stage.h"
struct a04_vendor_start {
    struct a04_vendor_m1_11_memory *memory;
    volatile UCHAR *core;
    void *trampoline_allocation;
    PHYSICAL_ADDRESS trampoline_logical;
    void *trampoline_cpu;
    uint64_t trampoline_dma;
    BOOLEAN attempted;
};
/* Caller owns a referenced DMA enabler and the mapped, powered register range.
 * Both must outlive any attempt, including failures: DMA remains quarantined.
 * power_is_stable and irq_is_masked must be proven by the caller, not inferred
 * from a readable BVNC or lack of interrupts.
 * PASSIVE_LEVEL; not a D0-exit or unload implementation. */
NTSTATUS a04_vendor_m1_11_windows_start(struct a04_vendor_start *s,
    struct a04_vendor_m1_11_memory *m, volatile UCHAR *core, ULONG core_bytes,
    uint64_t observed_bvnc, BOOLEAN power_is_stable, BOOLEAN irq_is_masked);
#endif
