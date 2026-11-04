/* SPDX-License-Identifier: MIT */
#ifndef A04_GE8300_RENDER_EXECUTION_H
#define A04_GE8300_RENDER_EXECUTION_H
#include <stddef.h>
#include <stdint.h>
/* OFFLINE FOUNDATION ONLY: this interface is not connected to Windows or a
 * hardware submission. It does not authenticate command bytes, acquire DMA,
 * synchronize caches or establish quiescence. Caller retains every resource. */
#define A04_RENDER_RANGE_READ 1u
#define A04_RENDER_RANGE_WRITE 2u
#define A04_RENDER_RANGE_EXEC 4u
struct a04_render_owned_range {
    uint64_t gpu_va, dma, size;
    unsigned access;
};
enum a04_render_execution_state {
    A04_RENDER_IDLE = 0, A04_RENDER_PUBLISHED, A04_RENDER_SUBMITTED,
    A04_RENDER_COMPLETED, A04_RENDER_RETIRED, A04_RENDER_POISONED
};
struct a04_render_execution {
    enum a04_render_execution_state state;
};
/* Callbacks must implement the actual DMA and firmware contracts. A failed
 * publish or later callback is potentially side-effecting and poisons the
 * state; resources may not be reused/freed until independent reset/quiescence.
 * A completion fence is not firmware cleanup. Both cleanup and MMU retire must
 * succeed before state RETIRED can be considered for resource release. */
struct a04_render_execution_ops {
    void *context;
    int (*sync_for_device)(void *context);
    int (*publish_mmu)(void *context);
    int (*submit)(void *context);
    int (*wait_geom)(void *context);
    int (*wait_frag)(void *context);
    int (*sync_for_cpu)(void *context);
    int (*firmware_cleanup)(void *context);
    int (*retire_mmu)(void *context);
};
/* Inputs are owned DMA ranges, not caller CPU pointers. All ranges must be
 * page aligned, disjoint in both GPU VA and DMA space, within 40/32 bits,
 * and GPU executable ranges must be read-only. 0 means structural validity;
 * it does not prove provenance, mapping existence, or required dependencies. */
int a04_render_validate_owned_ranges(const struct a04_render_owned_range *ranges,size_t count);
/* All callbacks required. Only IDLE may be started. Does not free anything. */
int a04_render_execute_owned(struct a04_render_execution *state,
    const struct a04_render_owned_range *ranges,size_t count,
    const struct a04_render_execution_ops *ops);
#endif
