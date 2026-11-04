#include "clock_owner.h"
static int enter(struct mt8167_clock_owner *o) {
    if (!o || !o->lock || !o->unlock) return -1;
    return o->lock(o->context);
}
int mt8167_clock_acquire(struct mt8167_clock_owner *o, unsigned client, uint32_t rate)
{
    uint32_t bit; int result=-1;
    if (client>=32 || !rate || enter(o)) return -1;
    bit=1u<<client;
    if (!o->online || o->poisoned || !o->verify_rate || (o->clients & bit) ||
        (o->clients && o->rate != rate)) goto done;
    /* Re-verify for each new client. No clock programming is performed. */
    if (o->verify_rate(o->context, rate)) { o->poisoned=1; goto done; }
    o->rate=rate; o->clients|=bit; result=0;
done:
    o->unlock(o->context);return result;
}
int mt8167_clock_release(struct mt8167_clock_owner *o, unsigned client)
{
    uint32_t bit;int result=-1;
    if (client>=32 || enter(o)) return -1;
    bit=1u<<client;
    if (o->clients & bit) { o->clients &= ~bit; if (!o->clients) o->rate=0;result=0; }
    o->unlock(o->context);return result;
}
int mt8167_clock_stop(struct mt8167_clock_owner *o)
{
    int result=-1;
    if (enter(o)) return -1;
    /* Stop admits no new clients, even if existing leases still must drain. */
    o->online=0;
    if (!o->clients) result=0;
    o->unlock(o->context);return result;
}
