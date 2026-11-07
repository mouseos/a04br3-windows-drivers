/* SPDX-License-Identifier: MIT */
#include "owner_broker.h"
#define SECOND_NS 1000000000ULL
#define PERIOD_NS (10ULL * SECOND_NS)

void a04_bq_broker_stop(struct a04_bq_broker *b)
{
    if (!b) return;
    b->stopped = 1;
    if (b->owner) b->owner->stopped = 1;
}
int a04_bq_broker_init(struct a04_bq_broker *b, struct a04_bq_owner *o,
                       const struct a04_bq_bus *charger, const struct a04_gauge_bus *gauge,
                       uint64_t now, uint64_t lifetime)
{
    if (!b || !o) return -1;
    /* A stopped or previously initialized broker cannot be restarted. */
    if (b->owner || b->stopped || b->entered) return -1;
    b->owner = o;
    if (!charger || !charger->now_ns || !charger->read || !charger->write_watchdog ||
        !gauge || !gauge->read_word_pec || o->stopped || (!o->firmware_kicks && o->handoff_pending!=2) ||
        !o->handoff_pending || !o->handoff_deadline || now >= o->handoff_deadline ||
        !lifetime || now >= lifetime) { a04_bq_broker_stop(b); return -1; }
    b->charger=charger; b->gauge=gauge; b->lifetime=lifetime;
    b->observed=now; b->next_due=now;
    return 0;
}
static enum a04_bq_broker_result run(struct a04_bq_broker *b, uint64_t seq,
                                     uint64_t deadline, struct a04_gauge_sample *out, int local)
{
    struct a04_gauge_sample sample;
    uint64_t now, start;
    int sample_result;
    enum a04_bq_broker_result result=A04_BROKER_STOPPED;
    if (!b || !b->owner || b->stopped || b->entered) return A04_BROKER_STOPPED;
    b->entered=1;
    now=b->charger->now_ns(b->charger->context);
    if (now < b->observed || now >= b->lifetime) goto stop;
    b->observed=now;
    /* Reserve a whole second for gauge IO; the first action is always a pet. */
    if (now >= b->next_due || b->next_due-now < SECOND_NS) {
        if (a04_bq_step(b->owner,b->charger,b->lifetime)!=A04_BQ_OK || b->stopped) goto stop;
        now=b->charger->now_ns(b->charger->context);
        if (now < b->observed || now < b->owner->last_kick || now >= b->lifetime ||
            b->owner->last_kick > UINT64_MAX-PERIOD_NS) goto stop;
        b->observed=now; b->next_due=b->owner->last_kick+PERIOD_NS;
        if (now >= b->next_due) goto stop;
    }
    if (!seq && !local) { result=A04_BROKER_OK; goto done; }
    if (!out || (!local && seq <= b->last_request) || deadline <= now || deadline > b->lifetime) {
        result=A04_BROKER_REJECTED; goto done;
    }
    if (!local) b->last_request=seq;
    start=now;
    sample_result=a04_gauge_read_sample(b->gauge,&sample);
    if (sample_result==A04_GAUGE_IO || b->stopped) goto stop;
    now=b->charger->now_ns(b->charger->context);
    if (now < start || now-start >= SECOND_NS || now >= b->next_due || now >= b->lifetime) goto stop;
    b->observed=now;
    if (sample_result!=A04_GAUGE_OK || now >= deadline) { result=A04_BROKER_REJECTED; goto done; }
    *out=sample; result=A04_BROKER_OK; goto done;
stop:
    a04_bq_broker_stop(b);
done:
    b->entered=0;
    return result;
}
enum a04_bq_broker_result a04_bq_broker_run(struct a04_bq_broker *b,uint64_t seq,
                                            uint64_t deadline,struct a04_gauge_sample *out)
{ return run(b,seq,deadline,out,0); }
enum a04_bq_broker_result a04_bq_broker_sample_local(struct a04_bq_broker *b,uint64_t deadline,
                                                     struct a04_gauge_sample *out)
{ return run(b,0,deadline,out,1); }
