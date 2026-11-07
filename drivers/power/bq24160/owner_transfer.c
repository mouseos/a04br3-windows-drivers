/* SPDX-License-Identifier: MIT */
#include "owner_transfer.h"
int a04_owner_transfer_run(struct a04_owner_transfer *s,const struct a04_transfer_ops *o,uint64_t deadline)
{
    uint64_t prior, now;
    int (*steps[4])(void *); unsigned i;
    if (!s || s->attempted || s->state!=A04_TRANSFER_OLD) return -1;
    s->attempted=1;s->state=A04_TRANSFER_FAILED;
    if (!o || !o->now_ns || !o->close_admission || !o->drain || !o->release || !o->acquire || !deadline) return -1;
    prior=o->now_ns(o->context);if (prior<s->last_observed || prior>=deadline) return -1;
    s->last_observed=prior;
    steps[0]=o->close_admission;steps[1]=o->drain;steps[2]=o->release;steps[3]=o->acquire;
    s->state=A04_TRANSFER_TRANSITION;
    for(i=0;i<4;++i) {
        if (steps[i](o->context)) goto fail;
        now=o->now_ns(o->context);if(now<prior || now>=deadline) goto fail;
        prior=now;s->last_observed=now;
    }
    s->state=A04_TRANSFER_NEW;return 0;
fail:
    s->state=A04_TRANSFER_FAILED;return -1;
}
