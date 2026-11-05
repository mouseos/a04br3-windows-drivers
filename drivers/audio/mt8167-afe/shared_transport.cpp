#include "shared_transport.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
int Mt8167SharedTransportStart(struct mt8167_audio_shared_client *shared,Mt8167Transport *transport,uint32_t period)
{
    if (!shared || !transport || transport->state!=AUDIO_READY ||
        !transport->dma || !transport->irq || !transport->engine ||
        !transport->notify || !transport->affinity || !period) return -1;
    if (mt8167_audio_shared_acquire(shared,48000)) return -1;
    int result=Mt8167TransportStartParentOwned(transport,period,shared);
    /* The transport quarantines before connecting IRQ or touching AFE.
     * Only failures before that boundary permit a quiescent lease release. */
    if (result && !shared->quarantined)
        (void)mt8167_audio_shared_release_unexposed(shared);
    return result;
}
#endif
