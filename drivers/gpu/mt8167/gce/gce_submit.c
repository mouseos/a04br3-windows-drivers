/* SPDX-License-Identifier: BSD-2-Clause-Patent
 * Source: MT8167 Linux cmdq_record.c and cmdq_core.c, including thread reset,
 * END/CURR/CFG/IRQ programming and per-thread IRQ acknowledgement. This core
 * is inert until an owner supplies real MMIO, barrier and authorization. */
#include "gce_submit.h"
#include "../wddm-display/mt8167_cmdq_packet.h"
#define GCE_SLOT_CYCLES 0x030u
#define GCE_RESET 0x100u
#define GCE_ENABLE 0x104u
#define GCE_SUSPEND 0x108u
#define GCE_STATUS 0x10cu
#define GCE_IRQ_STATUS 0x110u
#define GCE_IRQ_ENABLE 0x114u
#define GCE_CURRENT 0x120u
#define GCE_END 0x124u
#define GCE_CFG 0x140u
#define GCE_FIXED_END 0x1020ac00u
int a04_gce_prepare_primary(struct a04_gce_submit *s,uint32_t event,uint32_t l0,uint32_t l3)
{
    uint32_t words[A04_CMDQ_TWO_LAYER_FRAME_WORDS],enabled,irq,reset;unsigned i;
    if(!s || s->state!=A04_GCE_IDLE || !s->authorize || !s->read32 || !s->write32 ||
       !s->barrier || !s->delay || !s->command_cpu || !s->command_dma ||
       (s->command_dma&7u) || s->command_dma>UINT32_MAX-4095u ||
       ((uintptr_t)s->command_cpu&7u) || s->authorize(s->context)!=1 ||
       !a04_mt8167_cmdq_layer_frame(words,A04_CMDQ_TWO_LAYER_FRAME_WORDS,event,l0,l3,1) ||
       !a04_mt8167_cmdq_finalize(words,A04_CMDQ_TWO_LAYER_FRAME_WORDS,0))return -1;
    if(s->read32(s->context,GCE_ENABLE,&enabled) || s->read32(s->context,GCE_IRQ_STATUS,&irq) ||
       (enabled&1u) || (irq&0x13u))return -1;
    for(i=0;i<A04_CMDQ_TWO_LAYER_FRAME_WORDS;i++)s->command_cpu[i]=words[i];
    s->barrier(s->context);
    /* Hardware may observe reset or the command after the next write. */
    s->state=A04_GCE_RETAIN;
    if(s->write32(s->context,GCE_RESET,1u))return -1;
    for(i=0;i<1000u;i++) {
        if(s->read32(s->context,GCE_RESET,&reset))return -1;
        if(!(reset&1u))break;
        s->delay(s->context);
    }
    if(i==1000u)return -1;
    /* SLOT_CYCLES is controller-global: authorize() must cover all users. */
    if(s->write32(s->context,GCE_SLOT_CYCLES,0x3200u) ||
       s->write32(s->context,GCE_END,GCE_FIXED_END) ||
       s->write32(s->context,GCE_CURRENT,(uint32_t)s->command_dma) ||
       s->write32(s->context,GCE_CFG,0u))return -1;
    s->state=A04_GCE_PREPARED;return 0;
}
int a04_gce_kick_primary(struct a04_gce_submit *s)
{
    if(!s || s->state!=A04_GCE_PREPARED || !s->write32 || !s->barrier)return -1;
    /* The caller holds the IRQ lock: no ISR can see a partial transition. */
    s->state=A04_GCE_RETAIN;
    if(s->write32(s->context,GCE_IRQ_ENABLE,0x13u))return -1;
    s->barrier(s->context);
    if(s->write32(s->context,GCE_ENABLE,1u))return -1;
    s->state=A04_GCE_RUNNING;return 0;
}
int a04_gce_submit_irq(struct a04_gce_submit *s)
{
    uint32_t irq,enabled;
    if(!s || s->state!=A04_GCE_RUNNING || !s->read32 || !s->write32)return -1;
    if(s->read32(s->context,GCE_IRQ_STATUS,&irq)){s->state=A04_GCE_RETAIN;return -1;}
    if(!(irq&0x13u))return 1; /* interrupt was not this thread */
    if(s->read32(s->context,GCE_ENABLE,&enabled) || !(enabled&1u)) {
        s->state=A04_GCE_RETAIN;return -1;
    }
    s->state=A04_GCE_RETAIN;
    if(s->write32(s->context,GCE_IRQ_STATUS,~irq))return -1;
    if((irq&0x12u) || !(irq&1u))return -2;
    s->state=A04_GCE_IRQ_DONE;return 0;
}
void a04_gce_submit_timeout(struct a04_gce_submit *s)
{ if(s && (s->state==A04_GCE_RUNNING || s->state==A04_GCE_PREPARED))s->state=A04_GCE_RETAIN; }
int a04_gce_stop_completed(struct a04_gce_submit *s)
{
    uint32_t enabled,status,reset;unsigned i;
    if(!s || s->state!=A04_GCE_IRQ_DONE || !s->authorize || !s->read32 ||
       !s->write32 || !s->delay || s->authorize(s->context)!=1)return -1;
    /* Linux cmdq_core_suspend_HW_thread: suspend and wait for status bit 1;
     * disable_HW_thread then resets and disables.  Keep all resources even
     * on success: thread stop is not display-plane retirement. */
    s->state=A04_GCE_RETAIN;
    if(s->write32(s->context,GCE_SUSPEND,1u) ||
       s->read32(s->context,GCE_ENABLE,&enabled))return -1;
    if(enabled&1u) {
        for(i=0;i<1000u;i++) {
            if(s->read32(s->context,GCE_STATUS,&status))return -1;
            if(status&2u)break;
            s->delay(s->context);
        }
        if(i==1000u)return -1;
    }
    if(s->write32(s->context,GCE_RESET,1u))return -1;
    for(i=0;i<1000u;i++) {
        if(s->read32(s->context,GCE_RESET,&reset))return -1;
        if(!(reset&1u))break;
        s->delay(s->context);
    }
    if(i==1000u || s->write32(s->context,GCE_IRQ_ENABLE,0u) ||
       s->write32(s->context,GCE_ENABLE,0u) ||
       s->read32(s->context,GCE_ENABLE,&enabled) || (enabled&1u))return -1;
    s->state=A04_GCE_STOPPED;return 0;
}
