/* SPDX-License-Identifier: MIT */
#ifndef A04_BROKER_MAILBOX_H
#define A04_BROKER_MAILBOX_H
#include "owner_broker.h"

/* Local copied messages, not a shared-memory ABI. Transport must authenticate
 * this boot, copy atomically under exclusion, and provide cache coherency.
 * Neither a sequence number nor an epoch proves exclusive I2C ownership. */
struct a04_broker_request { uint64_t epoch, sequence, deadline; };
struct a04_broker_response {
    uint64_t epoch, sequence;
    enum a04_bq_broker_result result;
    struct a04_gauge_sample sample;
};
struct a04_broker_endpoint {
    struct a04_bq_broker *broker;
    uint64_t epoch, last_sequence;
    int closed, quiescing;
};
/* Caller guarantees the exclusive I2C owner is quiescent, owns this endpoint,
 * and has already established a valid boot-local epoch. Never call from two
 * threads or while the owner is processing another request. */
int a04_broker_endpoint_init(struct a04_broker_endpoint *, struct a04_bq_broker *, uint64_t epoch);
int a04_broker_endpoint_dispatch(struct a04_broker_endpoint *, const struct a04_broker_request *,
                                 struct a04_broker_response *);
/* The owner must call this independently of client traffic, with adequate
 * timing guarantees; the endpoint does not create or schedule a worker. */
int a04_broker_endpoint_maintain(struct a04_broker_endpoint *);
/* Under the same owner lock, close client admission without stopping the
 * maintenance worker. This is one-way and does not transfer ownership or
 * guarantee that the worker will continue running through D0 exit. */
int a04_broker_endpoint_quiesce(struct a04_broker_endpoint *);
/* Close stops maintenance; it does not transfer the bus or make it safe. */
void a04_broker_endpoint_close(struct a04_broker_endpoint *);
#endif
