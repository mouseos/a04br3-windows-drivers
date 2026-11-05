/* SPDX-License-Identifier: MIT */
#ifndef A04_BROKER_LIFECYCLE_H
#define A04_BROKER_LIFECYCLE_H
#include "broker_handoff.h"
enum a04_broker_lifecycle_state { A04_LIFE_NEW, A04_LIFE_LIVE, A04_LIFE_QUIESCING, A04_LIFE_HANDED_OFF, A04_LIFE_STOPPED };
struct a04_broker_lifecycle { struct a04_broker_endpoint *endpoint; enum a04_broker_lifecycle_state state; };
/* Caller holds the I2C owner lock throughout each call, including broker
 * timer and queue access. Input owner is already adopted and the maintenance
 * worker is armed before start publishes the endpoint. No function here proves
 * ownership, starts a worker, or changes platform power policy. */
int a04_broker_lifecycle_start(struct a04_broker_lifecycle *,struct a04_broker_endpoint *,
                               struct a04_bq_broker *,struct a04_bq_owner *,const struct a04_bq_bus *,
                               const struct a04_gauge_bus *,uint64_t now,uint64_t lifetime,uint64_t epoch);
int a04_broker_lifecycle_maintain(struct a04_broker_lifecycle *);
/* Returns 0 for a sample, 1 for unavailable data/deadline, -1 when stopped. */
int a04_broker_lifecycle_sample_local(struct a04_broker_lifecycle *,uint64_t deadline,struct a04_gauge_sample *);
int a04_broker_lifecycle_quiesce(struct a04_broker_lifecycle *);
int a04_broker_lifecycle_handoff(struct a04_broker_lifecycle *,struct a04_bq_owner *,
                                 struct a04_owner_transfer *,const struct a04_transfer_ops *,uint64_t deadline);
void a04_broker_lifecycle_stop(struct a04_broker_lifecycle *);
#endif
