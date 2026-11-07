#include "stream_transport.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
static int Start(Mt8167Transport *t, uint32_t periodBytes, struct mt8167_audio_shared_client *shared)
{
    struct mt8167_dl1_plan plan;
    bool parentOwned=shared!=NULL;
    if (!t || t->state != AUDIO_READY || !t->dma || !t->irq || !t->engine || (!parentOwned && !t->clock) ||
        (parentOwned && (!shared->referenced || !shared->leased || shared->quarantined)) ||
        !t->notify || t->irq->interrupt || t->dma->exposed || !t->dma->mdl ||
        t->dma->logical.QuadPart <= 0 || t->dma->logical.QuadPart > 0xffffffffLL ||
        !mt8167_dl1_plan((uint64_t)t->dma->logical.QuadPart,t->dma->size,periodBytes,48000,&plan)) return -1;
    if (!parentOwned && mt8167_clock_acquire(t->clock,t->clockClient,t->clockRate)) { t->state=AUDIO_FAILED;return -1; }
    if (!NT_SUCCESS(Mt8167CommonPrepareForDevice(t->dma))) {
        if (!parentOwned) (void)mt8167_clock_release(t->clock,t->clockClient);
        t->state=AUDIO_FAILED;return -1;
    }
    /* A connected ISR may run immediately. Retain the parent before any
     * such access, including a failed or ambiguous IRQ connection. */
    if (parentOwned && mt8167_audio_shared_quarantine(shared)) {
        t->state=AUDIO_FAILED;return -1;
    }
    if (!NT_SUCCESS(Mt8167ConnectIrq(t->irq,t->engine,t->vector,t->level,t->affinity,t->mode,t->notify,t->notifyContext))) {
        if (!parentOwned) (void)mt8167_clock_release(t->clock,t->clockClient);
        t->state=AUDIO_FAILED;return -1;
    }
    if (!NT_SUCCESS(Mt8167CommonMarkExposed(t->dma))) {
        Mt8167DisconnectIrq(t->irq);if (!parentOwned) (void)mt8167_clock_release(t->clock,t->clockClient);t->state=AUDIO_FAILED;return -1;
    }
    /* Exposure precedes every register write. Failed start may have reached
     * hardware; neither mapping nor clock lease can be released. */
    t->plan=plan;
    if (Mt8167SynchronizedStart(t->irq,&t->plan)) { t->state=AUDIO_QUARANTINED;return -1; }
    t->state=AUDIO_RUNNING;return 0;
}
int Mt8167TransportStart(Mt8167Transport *t,uint32_t periodBytes) { return Start(t,periodBytes,NULL); }
int Mt8167TransportStartParentOwned(Mt8167Transport *t,uint32_t periodBytes,struct mt8167_audio_shared_client *shared)
{ return shared ? Start(t,periodBytes,shared) : -1; }
int Mt8167TransportStartMaskedParent(Mt8167Transport *t,uint32_t periodBytes,
                                     struct mt8167_audio_shared_client *shared)
{
    struct mt8167_dl1_plan plan;
    /* Caller serializes with revoke and disconnect and owns a parent lease
     * whose mask/clear contract was satisfied before connecting this IRQ. */
    if (!t || t->state!=AUDIO_READY || !t->dma || !t->irq || !t->engine ||
        !shared || !shared->referenced || !shared->leased || shared->quarantined ||
        !t->irq->interrupt || t->irq->engine!=t->engine ||
        !InterlockedCompareExchange(&t->irq->active,0,0) ||
        InterlockedCompareExchange(&t->irq->fault,0,0) || t->engine->state!=MT8167_DL1_IDLE ||
        !t->dma->mdl || t->dma->exposed || t->dma->logical.QuadPart<=0 ||
        t->dma->logical.QuadPart>0xffffffffLL ||
        !mt8167_dl1_plan((uint64_t)t->dma->logical.QuadPart,t->dma->size,periodBytes,48000,&plan)) return -1;
    if (!NT_SUCCESS(Mt8167CommonPrepareForDevice(t->dma))) {t->state=AUDIO_FAILED;return -1;}
    /* Once DMA can be exposed, keep the parent, mapping and buffer even on
     * failure. IRQ was already connected while the parent kept it masked. */
    if (mt8167_audio_shared_quarantine(shared)) {t->state=AUDIO_FAILED;return -1;}
    if (!NT_SUCCESS(Mt8167CommonMarkExposed(t->dma))) {t->state=AUDIO_FAILED;return -1;}
    t->plan=plan;
    if (Mt8167SynchronizedStart(t->irq,&t->plan)) {t->state=AUDIO_QUARANTINED;return -1;}
    t->state=AUDIO_RUNNING;return 0;
}
int Mt8167TransportPosition(Mt8167Transport *t,uint32_t *offset)
{
    if (!t || !offset || t->state!=AUDIO_RUNNING || !t->irq) return -1;
    return Mt8167SynchronizedPosition(t->irq,&t->plan,offset);
}
int Mt8167TransportStop(Mt8167Transport *t)
{
    if (!t || t->state != AUDIO_RUNNING || !t->irq) return -1;
    if (Mt8167SynchronizedStop(t->irq)) { t->state=AUDIO_QUARANTINED;return -1; }
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
    if(Mt8167DrainStoppedIrq(t->irq)) {t->state=AUDIO_QUARANTINED;return -1;}
    /* Same DMA ring/SG/clock stay pinned. IRQ work drained, bus drain UNKNOWN. */
    t->state=AUDIO_STOPPED_RETAINED;return 0;
#else
    Mt8167DisconnectIrq(t->irq);
    /* No known drain proof exists. Keep clock and mapping. */
    t->state=AUDIO_QUARANTINED;return 0;
#endif
}
#if defined(A04_AUDIO_ENGINE_PCM) && A04_AUDIO_ENGINE_PCM
int Mt8167TransportRestartRetained(Mt8167Transport *t,BOOLEAN reset)
{
    if(!t || t->state!=AUDIO_STOPPED_RETAINED || !t->dma || !t->dma->exposed ||
       !t->dma->prepared || !t->irq || !t->irq->interrupt ||
       !t->engine || t->engine->state!=MT8167_DL1_STOPPED ||
       (uint64_t)t->dma->logical.QuadPart!=t->plan.base ||
       (uint64_t)t->plan.end+1!=(uint64_t)t->plan.base+t->dma->size)return -1;
    if(Mt8167SynchronizedRestart(t->irq,&t->plan,reset)) {
        t->state=AUDIO_QUARANTINED;return -1;
    }
    t->state=AUDIO_RUNNING;return 0;
}
#endif
#endif
