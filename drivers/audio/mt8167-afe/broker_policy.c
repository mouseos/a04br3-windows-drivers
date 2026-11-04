#include "broker_policy.h"
int mt8167_broker_acquire(struct mt8167_broker_policy *p, uint32_t client)
{
    if (!p || !client || p->poisoned || p->holder) return 0;
    p->holder = client;
    return 1;
}
int mt8167_broker_release(struct mt8167_broker_policy *p, uint32_t client,
                          int completed)
{
    if (!p || !client || p->holder != client) return 0;
    if (!completed) p->poisoned = 1;
    p->holder = 0;
    return 1;
}
