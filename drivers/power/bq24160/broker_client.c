/* SPDX-License-Identifier: MIT */
#include "broker_client.h"
#include <string.h>
void a04_broker_client_stop(struct a04_broker_client *c) { if (c) c->stopped=1; }
int a04_broker_client_init(struct a04_broker_client *c,const struct a04_client_transport *t,uint64_t epoch,uint64_t now)
{
    if (!c || c->epoch || c->stopped || c->entered) return -1;
    if (!t || !t->now_ns || !t->exchange || !epoch) { c->stopped=1; return -1; }
    c->transport=*t;c->epoch=epoch;c->observed=now;return 0;
}
int a04_broker_client_read(struct a04_broker_client *c,uint64_t deadline,struct a04_gauge_sample *out)
{
    struct a04_broker_request q;
    struct a04_broker_response r;
    uint64_t now;
    unsigned scaled;
    if (!c || !out || !c->epoch || c->stopped || c->entered) return -1;
    c->entered=1; now=c->transport.now_ns(c->transport.context);
    if (now<c->observed || !deadline || now>=deadline || c->sequence==UINT64_MAX) goto fail;
    c->observed=now; q.epoch=c->epoch;q.sequence=++c->sequence;q.deadline=deadline;
    memset(&r,0,sizeof(r));
    if (c->transport.exchange(c->transport.context,&q,&r)) goto fail;
    now=c->transport.now_ns(c->transport.context);
    if (c->stopped || now<c->observed || now>=deadline || r.epoch!=q.epoch ||
        r.sequence!=q.sequence || r.result!=A04_BROKER_OK ||
        r.sample.voltage_mv<2500 || r.sample.voltage_mv>4500 ||
        r.sample.indicator_tenths_percent>1000) goto fail;
    scaled=r.sample.indicator_tenths_percent<=100 ? 0 : r.sample.indicator_tenths_percent>=950 ? 100 :
           ((r.sample.indicator_tenths_percent-100u)*100u+425u)/850u;
    if (r.sample.percent!=scaled) goto fail;
    c->observed=now;*out=r.sample;c->entered=0;return 0;
fail:
    c->stopped=1;c->entered=0;return -1;
}
