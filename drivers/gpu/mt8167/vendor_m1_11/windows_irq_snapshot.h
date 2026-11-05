/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_IRQ_SNAPSHOT_H
#define A04_VENDOR_IRQ_SNAPSHOT_H
#include "windows_start.h"
struct a04_vendor_irq_snapshot { uint32_t count; BOOLEAN valid; };
/* Record a stable FW interrupt counter only while the FWCCB is empty. */
NTSTATUS a04_vendor_m1_11_windows_irq_snapshot(struct a04_vendor_start *start,
    BOOLEAN coherent_or_uncached,struct a04_vendor_irq_snapshot *snapshot);
NTSTATUS a04_vendor_m1_11_windows_fence(struct a04_vendor_start *start,
    uint32_t data_master,BOOLEAN coherent_or_uncached);
NTSTATUS a04_vendor_m1_11_windows_power_off(struct a04_vendor_start *start,
    BOOLEAN coherent_or_uncached);
/* Read-only diagnostics; values are a transient snapshot, not a quiescence proof. */
NTSTATUS a04_vendor_m1_11_power_debug_snapshot(struct a04_vendor_start *start, ULONG values[6]);
struct a04_vendor_fw_debug { ULONG read,write,count,words[2][10]; };
/* Bounded read-only snapshot; never advances or acknowledges a FW request. */
NTSTATUS a04_vendor_m1_11_fw_debug_snapshot(struct a04_vendor_start *start, struct a04_vendor_fw_debug *out);
#endif
