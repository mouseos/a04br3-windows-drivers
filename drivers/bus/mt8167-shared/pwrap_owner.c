#include "pwrap_owner.h"
static int enter(struct a04_pwrap_owner *o)
{
    if (!o || !o->lock || !o->unlock) return -1;
    return o->lock(o->lock_context);
}
int a04_pwrap_owner_start(struct a04_pwrap_owner *o)
{
    int result = -1;
    if (enter(o)) return -1;
    if (!o->online && !o->bus.poisoned && o->bus.read32 && o->bus.write32 &&
        o->bus.delay && o->bus.attempts) { o->online = 1; result = 0; }
    o->unlock(o->lock_context);
    return result;
}
int a04_pwrap_owner_stop(struct a04_pwrap_owner *o)
{
    if (enter(o)) return -1;
    o->online = 0;
    o->unlock(o->lock_context);
    return 0;
}
int a04_pwrap_owner_read_buttons(struct a04_pwrap_owner *o, uint16_t *status)
{
    int result = -1;
    if (!status || enter(o)) return -1;
    if (o->online) result = a04_pmic_chrstatus(&o->bus, status);
    o->unlock(o->lock_context);
    return result;
}
