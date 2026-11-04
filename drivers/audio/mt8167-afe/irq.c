#include "irq.h"
struct mt8167_dl1_irq_result mt8167_dl1_irq(uint32_t status,
                                             uint32_t dac_con0,
                                             int stream_running)
{
    struct mt8167_dl1_irq_result r = {0, 0};
    if (status & 1u) {
        r.acknowledge = 1u;
        r.notify = stream_running && ((dac_con0 & 2u) != 0u);
    }
    return r;
}
