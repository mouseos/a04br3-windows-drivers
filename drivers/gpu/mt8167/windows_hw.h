/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GE8300_WINDOWS_HW_H
#define A04_GE8300_WINDOWS_HW_H
#include <ntddk.h>
#include "firmware.h"
struct a04_gpu_dma_memory {
    PDMA_ADAPTER adapter;
    void *heap, *table;
    PHYSICAL_ADDRESS heap_logical, table_logical;
};
struct a04_gpu_register_context { volatile UCHAR *base; ULONG length; };
/* The adapter must be the device's bus-master DMA adapter; memory must be
 * zero-initialized and not already own allocations. Call at PASSIVE.
 * Stop the GPU and quiesce DMA before freeing its allocations. */
NTSTATUS a04_gpu_allocate_memory(PDMA_ADAPTER adapter, struct a04_gpu_dma_memory *memory);
void a04_gpu_free_memory(struct a04_gpu_dma_memory *memory);
/* Resources must already be assigned and mapped by the PnP owner. */
NTSTATUS a04_gpu_register_io_init(struct a04_gpu_register_context *context,
        volatile void *mapped_registers, ULONG length, struct a04_pvr_register_io *io);
void a04_gpu_dma_barrier(void);
/* device_writes=TRUE invalidates before the CPU reads; FALSE cleans before
 * the GPU reads. The caller uses only nonpaged common-buffer ranges. */
NTSTATUS a04_gpu_dma_sync(void *address, ULONG length, BOOLEAN device_writes);
#endif
