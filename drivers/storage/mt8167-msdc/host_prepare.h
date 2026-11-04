/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_HOST_PREPARE_H
#define A04_HOST_PREPARE_H
#include "pio.h"
/* Experimental, PASSIVE_LEVEL. The caller holds a non-revocable exclusive
 * parent/controller lease, stops all requests and establishes rail/pin state.
 * gate_clock changes only this controller's parent-owned clock gate; ungating
 * must restore exactly its previous state. source_hz_max is a proven upper
 * bound for the selected source, not a frequency inferred from register bits.
 * No caller may expose the controller until the entire card sequence succeeds.
 * Loss of hardware can still cause a synchronous bus exception. */
struct a04_host_prepare_ops {
    const struct a04_msdc_io *io;
    void *context;
    int (*owned_and_quiescent)(void *,uint32_t);
    int (*gate_clock)(void *,uint32_t,int);
    /* Verify current selected parent and a conservative frequency bound. */
    int (*verify_source)(void *,uint32_t,uint32_t);
    uint64_t (*now)(void *);
    uint64_t timeout_ticks;
    uint32_t source_hz_max;
};
struct a04_host_prepare_state { unsigned poisoned; };
enum a04_host_prepare_result { A04_HOST_OK, A04_HOST_INVALID, A04_HOST_POISONED,
    A04_HOST_UNOWNED, A04_HOST_DMA_ACTIVE, A04_HOST_TIMEOUT, A04_HOST_CLOCK,
    A04_HOST_VERIFY };
/* Failure after a register write poisons the state and requires external
 * recovery; no automatic rollback of a gate with unknown hardware state. */
enum a04_host_prepare_result a04_host_prepare(struct a04_host_prepare_state *,
    const struct a04_host_prepare_ops *,uint32_t generation,uint32_t poll_limit);
#endif
