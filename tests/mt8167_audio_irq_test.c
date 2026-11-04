#include <assert.h>
#include "irq.h"
int main(void) {
    struct mt8167_dl1_irq_result r;
    r = mt8167_dl1_irq(2, 2, 1); assert(!r.acknowledge && !r.notify);
    r = mt8167_dl1_irq(3, 2, 1); assert(r.acknowledge == 1 && r.notify);
    r = mt8167_dl1_irq(1, 0, 1); assert(r.acknowledge == 1 && !r.notify);
    r = mt8167_dl1_irq(1, 2, 0); assert(r.acknowledge == 1 && !r.notify);
    return 0;
}
