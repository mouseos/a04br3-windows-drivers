/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_GCE_SUBMIT_H
#define A04_GCE_SUBMIT_H
#include <stdint.h>
/* Internal executor only. The platform owner serializes calls and must prove
 * clock/power, GCE-wide register ownership, IRQ/thread/event ownership, the
 * display address, DMA mapping and command-buffer lifetime in authorize(). */
struct a04_gce_submit {
    void *context;
    int (*authorize)(void *);
    int (*read32)(void *,uint32_t,uint32_t *);
    int (*write32)(void *,uint32_t,uint32_t);
    void (*barrier)(void *);
    void (*delay)(void *);
    uint32_t *command_cpu;
    uint64_t command_dma;
    uint32_t state;
};
#define A04_GCE_IDLE 0u
#define A04_GCE_RETAIN 1u
#define A04_GCE_RUNNING 2u
#define A04_GCE_IRQ_DONE 3u
/* Success means a task was started, not that OVL latched a frame. All
 * failures after the first register write retain both mappings. */
int a04_gce_submit_primary(struct a04_gce_submit *,uint32_t event,uint32_t l0,uint32_t l3);
/* IRQ is only for the owned thread; an EOC is not an OVL latch proof. */
int a04_gce_submit_irq(struct a04_gce_submit *);
void a04_gce_submit_timeout(struct a04_gce_submit *);
#endif
