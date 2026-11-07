/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VENDOR_UNCACHED_H
#define A04_VENDOR_UNCACHED_H
#include <ntddk.h>
struct a04_vendor_uncached {
    PDMA_ADAPTER adapter;
    void *cpu;
    PHYSICAL_ADDRESS logical;
    ULONG bytes;
};
/* adapter must be a referenced version-3 IoGetDmaAdapter result, and remain
 * alive until free. This is not safe for an arbitrary WDF version-2 adapter.
 * ARM uncached common buffers are Device memory; only appropriately aligned
 * device-memory accesses may touch them. PASSIVE_LEVEL. */
NTSTATUS a04_vendor_uncached_allocate(struct a04_vendor_uncached *buffer,
    PDMA_ADAPTER adapter,ULONG bytes);
void a04_vendor_uncached_free(struct a04_vendor_uncached *buffer);
#endif
