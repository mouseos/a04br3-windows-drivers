/* SPDX-License-Identifier: MIT */
#include "broker_mailbox.h"
#include <string.h>
int a04_broker_endpoint_init(struct a04_broker_endpoint *e, struct a04_bq_broker *b, uint64_t epoch)
{
    if (!e || !b || !epoch || e->broker || e->closed || e->quiescing || b->stopped || !b->owner) return -1;
    e->broker=b; e->epoch=epoch; return 0;
}
int a04_broker_endpoint_quiesce(struct a04_broker_endpoint *e)
{
    if (!e || !e->broker || e->closed || e->quiescing ||
        e->broker->stopped || e->broker->entered) return -1;
    e->quiescing=1;
    return 0;
}
void a04_broker_endpoint_close(struct a04_broker_endpoint *e)
{
    if (!e) return;
    e->closed=1;
    if (e->broker) a04_bq_broker_stop(e->broker);
}
int a04_broker_endpoint_maintain(struct a04_broker_endpoint *e)
{
    if (!e || !e->broker || e->closed) return -1;
    if (a04_bq_broker_run(e->broker,0,0,0)!=A04_BROKER_OK) {
        a04_broker_endpoint_close(e); return -1;
    }
    return 0;
}
int a04_broker_endpoint_dispatch(struct a04_broker_endpoint *e, const struct a04_broker_request *req,
                                 struct a04_broker_response *out)
{
    struct a04_broker_response result;
    if (!out) return -1;
    memset(out,0,sizeof(*out));
    if (!e || !req || !e->broker || e->closed || e->quiescing || e->broker->stopped ||
        req->epoch!=e->epoch || !req->sequence || req->sequence<=e->last_sequence ||
        !req->deadline) return -1;
    memset(&result,0,sizeof(result));
    result.epoch=e->epoch; result.sequence=req->sequence;
    e->last_sequence=req->sequence;
    result.result=a04_bq_broker_run(e->broker,req->sequence,req->deadline,&result.sample);
    if (result.result==A04_BROKER_STOPPED) e->closed=1;
    *out=result;
    return 0;
}
