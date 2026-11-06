/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_WINDOWS_SHARED_TABLES_H
#define A04_WINDOWS_SHARED_TABLES_H
#include "windows_shared_scanout.h"
#include "windows_copy_scratch.h"
/* Inactive GPU DMA tables only; never linked to a live PC root. Caller holds
 * the provider lifetime lock and the WDDM shared-MDL lease. */
#if A04_GE8300_SHARED_WIDE_FRAME
#define A04_SHARED_TABLE_COUNT 6u
#else
#define A04_SHARED_TABLE_COUNT 4u
#endif
struct a04_shared_tables {
    struct a04_vendor_uncached table[A04_SHARED_TABLE_COUNT];
#if A04_GE8300_SHARED_WIDE_FRAME
    struct a04_vendor_uncached wide_source;
#endif
    BOOLEAN ready,quarantined,published,mmu_ack;
};
NTSTATUS a04_shared_tables_stage(struct a04_shared_tables *,struct a04_shared_scanout *,
    struct a04_vendor_render_cold *,struct a04_vendor_copy_scratch *);
NTSTATUS a04_shared_tables_release(struct a04_shared_tables *);
struct a04_vendor_start;
/* Experimental irreversible root publication. Caller holds lifetime lock.
 * A root write retains all mappings until independent quiescence, even when
 * the MMU command acknowledges. */
NTSTATUS a04_shared_tables_publish(struct a04_shared_tables *,struct a04_shared_scanout *,
    struct a04_vendor_render_cold *,struct a04_vendor_start *);
#endif
