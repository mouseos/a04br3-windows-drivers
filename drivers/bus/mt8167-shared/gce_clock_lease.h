/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GCE_CLOCK_LEASE_H
#define A04_GCE_CLOCK_LEASE_H
#include <stdint.h>
/* Caller holds the parent lock and keeps the parent powered. An assigned MMIO
 * range is not proof that firmware, secure world, and other clients released
 * the GCE clock. ownership_proven must be supplied by that platform contract. */
struct a04_gce_clock_lease {
    void *context;
    int (*read32)(void *,unsigned,uint32_t *);
    int (*write32)(void *,unsigned,uint32_t);
    uint32_t generation,active;
    int online,ownership_proven,restore_gated,poisoned;
};
/* Failure after reservation retains *token and poisons the lease. */
int a04_gce_clock_acquire(struct a04_gce_clock_lease *,uint32_t *token);
/* quiescent_proven must cover GCE DMA, threads, IRQ and all other consumers.
 * A fence, timeout or object reference alone is not this proof. */
int a04_gce_clock_release(struct a04_gce_clock_lease *,uint32_t token,int quiescent_proven);
#endif
