/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GE8300_TRANSFER_MAPPING_H
#define A04_GE8300_TRANSFER_MAPPING_H
#include <stddef.h>
#include <stdint.h>

#define A04_TRANSFER_PAGES 6u
#define A04_TRANSFER_SOURCE_VA UINT64_C(0x80000b6000)
#define A04_TRANSFER_DEST_VA UINT64_C(0x80000bc000)

struct a04_transfer_range { uint64_t address, length; };
struct a04_transfer_mapping_ops {
    int (*wait_idle)(void *context); /* fence AND ring read offset */
    int (*sync_table)(void *context, void *table, size_t size);
    int (*invalidate)(void *context); /* waits for firmware token ACK */
};
enum a04_transfer_mapping_state {
    A04_TRANSFER_SCRATCH = 0, A04_TRANSFER_EXTERNAL, A04_TRANSFER_POISONED
};
struct a04_transfer_mapping {
    uint64_t *leaf;
    uint64_t scratch_source[A04_TRANSFER_PAGES], scratch_dest[A04_TRANSFER_PAGES];
    enum a04_transfer_mapping_state state;
    struct a04_transfer_mapping_ops ops;
    void *context;
};
/* The GPU context must already map the scratch pages at both fixed VAs and
 * they must be synchronized and invalidated. The caller serializes all
 * queue/MMU operations, retains scratch memory for the context lifetime and
 * keeps every protected DMA range immutable throughout each operation.
 * Source/destination mappings are OS DMA addresses, already pinned, and all
 * mappings remain retained if this function or restore poisons the context.
 * A poisoned context may only be disposed after confirmed GPU DMA quiescence.
 * These functions do not acquire, synchronize or release external DMA. */
int a04_transfer_map(struct a04_transfer_mapping *mapping,
    const uint64_t source[A04_TRANSFER_PAGES], const uint64_t dest[A04_TRANSFER_PAGES],
    const struct a04_transfer_range *protected_ranges, size_t protected_count);
/* Restore scratch and wait for invalidate ACK before releasing external DMA. */
int a04_transfer_restore(struct a04_transfer_mapping *mapping);
#endif
