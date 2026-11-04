#include "stream_transport.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
int Mt8167TransportStart(Mt8167Transport *t, uint32_t periodBytes)
{
    struct mt8167_dl1_plan plan;
    if (!t || t->state != AUDIO_READY || !t->dma || !t->irq || !t->engine || !t->clock ||
        !t->notify || t->irq->interrupt || t->dma->exposed || !t->dma->mdl ||
        t->dma->logical.QuadPart <= 0 || t->dma->logical.QuadPart > 0xffffffffLL ||
        !mt8167_dl1_plan((uint64_t)t->dma->logical.QuadPart,t->dma->size,periodBytes,48000,&plan)) return -1;
    if (mt8167_clock_acquire(t->clock,t->clockClient,t->clockRate)) { t->state=AUDIO_FAILED;return -1; }
    if (!NT_SUCCESS(Mt8167CommonPrepareForDevice(t->dma))) {
        (void)mt8167_clock_release(t->clock,t->clockClient);t->state=AUDIO_FAILED;return -1;
    }
    if (!NT_SUCCESS(Mt8167ConnectIrq(t->irq,t->engine,t->vector,t->level,t->affinity,t->mode,t->notify,t->notifyContext))) {
        (void)mt8167_clock_release(t->clock,t->clockClient);t->state=AUDIO_FAILED;return -1;
    }
    if (!NT_SUCCESS(Mt8167CommonMarkExposed(t->dma))) {
        Mt8167DisconnectIrq(t->irq);(void)mt8167_clock_release(t->clock,t->clockClient);t->state=AUDIO_FAILED;return -1;
    }
    /* Exposure precedes every register write. Failed start may have reached
     * hardware; neither mapping nor clock lease can be released. */
    if (Mt8167SynchronizedStart(t->irq,&plan)) { t->state=AUDIO_QUARANTINED;return -1; }
    t->state=AUDIO_RUNNING;return 0;
}
int Mt8167TransportStop(Mt8167Transport *t)
{
    if (!t || t->state != AUDIO_RUNNING || !t->irq) return -1;
    if (Mt8167SynchronizedStop(t->irq)) { t->state=AUDIO_QUARANTINED;return -1; }
    Mt8167DisconnectIrq(t->irq);
    /* No known drain proof exists. Keep clock and mapping. */
    t->state=AUDIO_QUARANTINED;return 0;
}
#endif
