/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_VEMC_RESTORE_H
#define A04_VEMC_RESTORE_H
#include <stdint.h>
/* Offline-only, PASSIVE_LEVEL prototype. Caller must hold a non-revocable
 * exclusive PWRAP/rail lease and the shared parent lock for the full call,
 * with every consumer quiesced and VIO18 owned. No Windows caller exists.
 * A software held callback cannot make an asynchronous bus abort safe. */
struct a04_vemc_io {
    void *context;
    int (*held_and_quiescent)(void *,uint32_t);
    int (*read32)(void *,unsigned,uint32_t *);
    int (*write32)(void *,unsigned,uint32_t);
    int (*wait_us)(void *,unsigned);
    uint64_t (*now)(void *);
    uint64_t timeout_ticks;
    /* Trusted conversion of at least 264 us to the same monotonic clock. */
    uint64_t ramp_ticks;
};
struct a04_vemc_state { unsigned poisoned; };
struct a04_vemc_result { uint16_t selector,control,vio18; unsigned wrote_enable; };
enum a04_vemc_status { A04_VEMC_OK,A04_VEMC_INVALID,A04_VEMC_UNOWNED,
    A04_VEMC_TIMEOUT,A04_VEMC_IO,A04_VEMC_UNSAFE,A04_VEMC_MISMATCH,A04_VEMC_POISONED };
/* Only allows VEMC bit14 0->1; preserves selector and VIO18, never power
 * cycles. expected_selector is 0 or 1, from a separately trusted policy.
 * Failure poisons the state; a write may have completed even after failure.
 * QI/enable bits are register observations, not proof of actual voltage. */
enum a04_vemc_status a04_vemc_restore(struct a04_vemc_state *,const struct a04_vemc_io *,
    uint32_t generation,unsigned expected_selector,unsigned poll_limit,struct a04_vemc_result *);
#endif
