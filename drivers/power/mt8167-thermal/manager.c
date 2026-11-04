/* SPDX-License-Identifier: GPL-2.0-only */
#include "manager.h"
#include <stddef.h>

int a04_thermal_start(struct a04_thermal_manager *m, const struct a04_thermal_ops *o,
                      const struct a04_thermal_policy *p)
{
    struct a04_thermal_cal cal;
    if (!m) return -1;
    m->ready=0;
    if (!o || !p || !o->read_devinfo || !o->acquire || !o->release || !o->read_fresh || !o->apply ||
        !p->max_age_ms || p->release_mc>=p->limit_mc || p->limit_mc>=p->halt_mc) {
        if (o && o->apply) (void)o->apply(o->ctx,A04_TEMP_HALT);
        return -1;
    }
    if (o->apply(o->ctx,A04_TEMP_LIMIT)) { (void)o->apply(o->ctx,A04_TEMP_HALT); return -1; }
    if (o->acquire(o->ctx)) { (void)o->apply(o->ctx,A04_TEMP_HALT); return -1; }
    if (a04_thermal_calibrate(&cal,o->read_devinfo,o->ctx)) { o->release(o->ctx); (void)o->apply(o->ctx,A04_TEMP_HALT); return -1; }
    o->release(o->ctx);
    m->cal=cal; m->policy=*p; m->policy.limited=1; m->ready=1;
    return 0;
}

int a04_thermal_poll(struct a04_thermal_manager *m, const struct a04_thermal_ops *o, uint64_t now,
                     struct a04_thermal_sample *out)
{
    uint32_t raw[3]; uint64_t measured=0; struct a04_thermal_sample candidate;
    enum a04_thermal_action action=A04_TEMP_HALT; int valid=0, applied;
    if (!m || !o || !o->acquire || !o->release || !o->read_fresh || !o->apply) return -1;
    if (m->ready && !o->acquire(o->ctx)) {
        if (!o->read_fresh(o->ctx,raw,&measured) &&
            !a04_thermal_sample(&m->cal,raw,measured,1,&candidate)) {
            action=a04_thermal_decide(&m->policy,&candidate,now,1);
            valid=action!=A04_TEMP_HALT;
        }
        o->release(o->ctx);
    }
    applied=o->apply(o->ctx,action);
    if (applied || !valid) { if (applied && action!=A04_TEMP_HALT) (void)o->apply(o->ctx,A04_TEMP_HALT); m->ready=0; return -1; }
    if (out) *out=candidate;
    return 0;
}
