#ifndef MT8167_AFE_IRQ_H
#define MT8167_AFE_IRQ_H
#include <stdint.h>
/* Pure classification only. Caller must own and synchronize the entire AFE
 * interrupt source. No MMIO, acknowledge or notification is performed here. */
struct mt8167_dl1_irq_result { uint32_t acknowledge; int notify; };
struct mt8167_dl1_irq_result mt8167_dl1_irq(uint32_t status,
                                             uint32_t dac_con0,
                                             int stream_running);
#endif
