/* SPDX-License-Identifier: MIT */
#ifndef A04_BQ_OWNER_BROKER_H
#define A04_BQ_OWNER_BROKER_H
#include "charger.h"
#include "../lc709203f/gauge.h"

/* Zero-initialize before init. Single-threaded policy only: caller must serialize every entry, own I2C2,
 * arrange timely invocation, and establish an independent lifetime and clock.
 * This is not a mailbox, power-state guarantee, or hardware admission gate. */
struct a04_bq_broker {
    struct a04_bq_owner *owner;
    const struct a04_bq_bus *charger;
    const struct a04_gauge_bus *gauge;
    uint64_t lifetime, observed, next_due, last_request;
    int stopped, entered;
};
enum a04_bq_broker_result { A04_BROKER_OK, A04_BROKER_REJECTED, A04_BROKER_STOPPED };
int a04_bq_broker_init(struct a04_bq_broker *, struct a04_bq_owner *,
                       const struct a04_bq_bus *, const struct a04_gauge_bus *,
                       uint64_t now, uint64_t lifetime);
/* seq=0 runs maintenance only; otherwise require an increasing, nonzero seq
 * and an absolute deadline. Output changes only on a completed valid sample. */
enum a04_bq_broker_result a04_bq_broker_run(struct a04_bq_broker *, uint64_t seq,
                                            uint64_t request_deadline,
                                            struct a04_gauge_sample *output);
void a04_bq_broker_stop(struct a04_bq_broker *);
#endif
