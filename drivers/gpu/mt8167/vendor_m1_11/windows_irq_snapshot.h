/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_IRQ_SNAPSHOT_H
#define A04_VENDOR_IRQ_SNAPSHOT_H
#include "windows_start.h"
struct a04_vendor_irq_snapshot { uint32_t count; BOOLEAN valid; };
/* Record a stable FW interrupt counter only while the FWCCB is empty. */
NTSTATUS a04_vendor_m1_11_windows_irq_snapshot(struct a04_vendor_start *start,
    BOOLEAN coherent_or_uncached,struct a04_vendor_irq_snapshot *snapshot);
#endif
