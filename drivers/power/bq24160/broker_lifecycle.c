/* SPDX-License-Identifier: MIT */
#include "broker_lifecycle.h"
void a04_broker_lifecycle_stop(struct a04_broker_lifecycle *l)
{
    if (!l || l->state==A04_LIFE_HANDED_OFF || l->state==A04_LIFE_STOPPED) return;
    if (l->endpoint) a04_broker_endpoint_close(l->endpoint);
    l->state=A04_LIFE_STOPPED;
}
int a04_broker_lifecycle_start(struct a04_broker_lifecycle *l,struct a04_broker_endpoint *e,
                               struct a04_bq_broker *b,struct a04_bq_owner *o,const struct a04_bq_bus *charger,
                               const struct a04_gauge_bus *gauge,uint64_t now,uint64_t lifetime,uint64_t epoch)
{
    if (!l || l->state!=A04_LIFE_NEW || l->endpoint || !e || !b || !o ||
        e->broker || e->closed || e->quiescing || b->owner || b->stopped || b->entered) return -1;
    l->endpoint=e;
    if (!epoch || a04_bq_broker_init(b,o,charger,gauge,now,lifetime) || a04_broker_endpoint_init(e,b,epoch)) {
        a04_bq_broker_stop(b);a04_broker_lifecycle_stop(l);return -1;
    }
    l->state=A04_LIFE_LIVE;return 0;
}
int a04_broker_lifecycle_maintain(struct a04_broker_lifecycle *l)
{
    if (!l || (l->state!=A04_LIFE_LIVE && l->state!=A04_LIFE_QUIESCING)) return -1;
    if (a04_broker_endpoint_maintain(l->endpoint)) {a04_broker_lifecycle_stop(l);return -1;}
    return 0;
}
int a04_broker_lifecycle_sample_local(struct a04_broker_lifecycle *l,uint64_t deadline,struct a04_gauge_sample *out)
{
    if (!l || l->state!=A04_LIFE_LIVE || !l->endpoint || !l->endpoint->broker) return -1;
    enum a04_bq_broker_result result;
    result=a04_bq_broker_sample_local(l->endpoint->broker,deadline,out);
    if (result==A04_BROKER_STOPPED) { a04_broker_lifecycle_stop(l);return -1; }
    return result==A04_BROKER_OK ? 0 : 1;
}
int a04_broker_lifecycle_quiesce(struct a04_broker_lifecycle *l)
{
    if (!l || l->state!=A04_LIFE_LIVE || a04_broker_endpoint_quiesce(l->endpoint)) return -1;
    l->state=A04_LIFE_QUIESCING;return 0;
}
int a04_broker_lifecycle_handoff(struct a04_broker_lifecycle *l,struct a04_bq_owner *destination,
                                 struct a04_owner_transfer *t,const struct a04_transfer_ops *ops,uint64_t deadline)
{
    if (!l || l->state!=A04_LIFE_QUIESCING) return -1;
    if (a04_broker_handoff(l->endpoint,destination,t,ops,deadline)) {
        if (l->endpoint->closed) l->state=A04_LIFE_STOPPED;
        return -1;
    }
    l->state=A04_LIFE_HANDED_OFF;return 0;
}

int a04_broker_lifecycle_default(struct a04_broker_lifecycle *l,int (*reset)(void *),void *context)
{
    struct a04_bq_broker *b;
    uint64_t now;
    int eligible;
    if (!l || l->state!=A04_LIFE_QUIESCING || !l->endpoint ||
        !(b=l->endpoint->broker) || !b->owner || !b->charger || !b->charger->now_ns) return -1;
    now=b->charger->now_ns(b->charger->context);
    eligible=reset && !l->endpoint->closed && !b->stopped && !b->entered &&
        !b->owner->stopped && !b->owner->handoff_pending && b->owner->kicks &&
        now>=b->observed && now>=b->owner->last_kick && now<b->next_due && now<b->lifetime;
    /* Close before writing; failure leaves mode unknown and cannot be retried. */
    a04_broker_lifecycle_stop(l);
    if (!eligible) return -1;
    return reset(context) ? -1 : 0;
}
