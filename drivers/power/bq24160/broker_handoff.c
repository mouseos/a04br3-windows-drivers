/* SPDX-License-Identifier: MIT */
#include "broker_handoff.h"
#define PERIOD_NS 10000000000ULL
int a04_broker_handoff(struct a04_broker_endpoint *e,struct a04_bq_owner *destination,
                       struct a04_owner_transfer *transfer,const struct a04_transfer_ops *ops,uint64_t deadline)
{
    struct a04_bq_broker *broker;
    struct a04_bq_owner snapshot;
    uint64_t now,limit;
    if (!e || !destination || !transfer || !ops || !ops->now_ns || !e->epoch || !e->quiescing || e->closed ||
        !(broker=e->broker) || !broker->owner || broker->stopped || broker->entered ||
        destination==broker->owner || destination->stopped || destination->attempts || destination->kicks ||
        destination->handoff_pending || destination->firmware_kicks || destination->last_kick ||
        destination->handoff_deadline || destination->fault_history || destination->first.valid ||
        destination->latest.valid || destination->firmware_first.valid || destination->firmware_latest.valid ||
        !broker->owner->kicks || !broker->owner->firmware_kicks ||
        broker->owner->stopped || broker->owner->handoff_pending || transfer->attempted ||
        transfer->state!=A04_TRANSFER_OLD || !deadline || !broker->next_due || !broker->lifetime ||
        broker->owner->last_kick>UINT64_MAX-PERIOD_NS ||
        broker->next_due!=broker->owner->last_kick+PERIOD_NS)
        return -1;
    now=ops->now_ns(ops->context);
    limit=deadline<broker->lifetime?deadline:broker->lifetime;
    if (broker->next_due<limit) limit=broker->next_due;
    if (now<transfer->last_observed || now<broker->observed || now<broker->owner->last_kick || now>=limit) return -1;
    snapshot=*broker->owner;
    transfer->last_observed=now;
    if (a04_owner_transfer_run(transfer,ops,limit)) {
        a04_broker_endpoint_close(e);destination->stopped=1;return -1;
    }
    now=ops->now_ns(ops->context);
    if (now<transfer->last_observed || now<broker->observed || now<snapshot.last_kick || now>=limit) {
        transfer->state=A04_TRANSFER_FAILED;
        a04_broker_endpoint_close(e);destination->stopped=1;return -1;
    }
    /* Preserve the original earlier deadline through the first new pet. */
    snapshot.handoff_pending=1;snapshot.handoff_deadline=limit;
    a04_broker_endpoint_close(e);
    *destination=snapshot;
    return 0;
}
