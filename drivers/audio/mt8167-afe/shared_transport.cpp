#include "shared_transport.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
int Mt8167SharedTransportStart(struct mt8167_audio_shared_client *shared,Mt8167Transport *transport,uint32_t period)
{
    if (!shared || !transport || transport->state!=AUDIO_READY ||
        !transport->dma || !transport->irq || !transport->engine ||
        !transport->notify || !transport->affinity || !period) return -1;
    if (mt8167_audio_shared_acquire(shared,48000)) return -1;
    /* Quarantine before transport can publish any register, even if a later
     * validation fails. There is no supported drain/recovery release yet. */
    if (mt8167_audio_shared_quarantine(shared)) return -1;
    return Mt8167TransportStartParentOwned(transport,period);
}
#endif
