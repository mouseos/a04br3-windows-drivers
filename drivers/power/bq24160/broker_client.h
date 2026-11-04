/* SPDX-License-Identifier: MIT */
#ifndef A04_BROKER_CLIENT_H
#define A04_BROKER_CLIENT_H
#include "broker_mailbox.h"
/* A bounded, synchronous transport. The implementation must copy requests,
 * serialize access, handle cache coherency, and never touch I2C2. */
struct a04_client_transport {
    void *context;
    uint64_t (*now_ns)(void *);
    int (*exchange)(void *, const struct a04_broker_request *, struct a04_broker_response *);
};
struct a04_broker_client {
    struct a04_client_transport transport;
    uint64_t epoch, sequence, observed;
    int stopped, entered;
};
int a04_broker_client_init(struct a04_broker_client *, const struct a04_client_transport *, uint64_t epoch, uint64_t now);
int a04_broker_client_read(struct a04_broker_client *, uint64_t deadline, struct a04_gauge_sample *);
void a04_broker_client_stop(struct a04_broker_client *);
#endif
