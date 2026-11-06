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
#define A04_GCE_PREPARED 4u
#define A04_GCE_STOPPED 5u
/* Prepare is PASSIVE_LEVEL and may wait for reset. It must run while IRQs
 * for this thread are disabled and the lease is held. Once a register write
 * is attempted, failure retains the buffers. */
int a04_gce_prepare_primary(struct a04_gce_submit *,uint32_t event,uint32_t l0,uint32_t l3);
/* Kick must run under the same WDF interrupt lock as a04_gce_submit_irq.
 * It never sleeps. Success means started, not that OVL latched a frame. */
int a04_gce_kick_primary(struct a04_gce_submit *);
/* IRQ result: 0=EOC acknowledged, 1=not ours, -2=error IRQ acknowledged,
 * -1=uncertain read/ack. An EOC is not an OVL latch proof. */
int a04_gce_submit_irq(struct a04_gce_submit *);
void a04_gce_submit_timeout(struct a04_gce_submit *);
/* PASSIVE_LEVEL, with the thread/IRQ and DMA lifetime exclusively held.
 * Stopping the GCE thread does not prove an OVL latch or surface retirement. */
int a04_gce_stop_completed(struct a04_gce_submit *);
/* Reuse the stopped command buffer for a saved two-layer transaction only
 * after an independent provider validates GCE DMA quiescence and both saved
 * display mappings. The caller holds the same clock/thread/event lease. A
 * successful prepare is not a restore latch or surface-retirement proof. */
int a04_gce_prepare_restore(struct a04_gce_submit *,uint32_t event,uint32_t l0,uint32_t l3,
                            int (*validate)(void *,uint32_t,uint32_t));
#endif
