#include <assert.h>
#include "broker_policy.h"
int main(void) {
    struct mt8167_broker_policy p = {0, 0};
    assert(!mt8167_broker_acquire(&p, 0));
    assert(mt8167_broker_acquire(&p, 1));
    assert(!mt8167_broker_acquire(&p, 1));
    assert(!mt8167_broker_acquire(&p, 2));
    assert(!mt8167_broker_release(&p, 2, 1));
    assert(p.holder == 1);
    assert(mt8167_broker_release(&p, 1, 1));
    assert(mt8167_broker_acquire(&p, 2));
    assert(mt8167_broker_release(&p, 2, 0));
    assert(p.poisoned && !p.holder);
    assert(!mt8167_broker_acquire(&p, 1));
    assert(!mt8167_broker_release(&p, 2, 1));
    return 0;
}
