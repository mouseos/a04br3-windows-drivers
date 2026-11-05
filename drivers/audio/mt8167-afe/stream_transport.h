#ifndef MT8167_STREAM_TRANSPORT_H
#define MT8167_STREAM_TRANSPORT_H
#include "common_dma.h"
#include "irq_windows.h"
#include "shared_audio.h"
#include "../../bus/mt8167-shared/clock_owner.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
/* Internal prototype; no PnP or WaveRT caller exists. Caller owns & serializes
 * the whole object at PASSIVE_LEVEL and must provide genuine resource owners,
 * safe clocks, codec mute, and a cache-coherent mapped buffer. The fixed audio
 * buffer must not be written concurrently during the initial publish. */
enum Mt8167TransportState { AUDIO_READY, AUDIO_RUNNING, AUDIO_QUARANTINED, AUDIO_FAILED };
struct Mt8167Transport {
    Mt8167CommonDma *dma;
    Mt8167Irq *irq;
    struct mt8167_dl1_engine *engine;
    struct mt8167_clock_owner *clock;
    unsigned clockClient;
    uint32_t clockRate;
    ULONG vector;
    KIRQL level;
    KAFFINITY affinity;
    KINTERRUPT_MODE mode;
    MT8167_IRQ_NOTIFY notify;
    void *notifyContext;
    struct mt8167_dl1_plan plan;
    enum Mt8167TransportState state;
};
/* No caller has permission to start until the unresolved platform prerequisites
 * have independent evidence. On any exposure retain DMA and clock until reset. */
int Mt8167TransportStart(Mt8167Transport *t, uint32_t periodBytes);
/* The caller holds the shared parent's exclusive clock and muted-codec lease.
 * It becomes permanently quarantined before an IRQ can be connected. */
int Mt8167TransportStartParentOwned(Mt8167Transport *t, uint32_t periodBytes,
                                   struct mt8167_audio_shared_client *shared);
/* Returns ring offset, not a count of elapsed periods. Caller excludes stop,
 * disconnect and PnP removal and keeps the owner and mapping referenced. */
int Mt8167TransportPosition(Mt8167Transport *t,uint32_t *offset);
/* A successful stop only disables AFE and disconnects the IRQ. It does not
 * certify DMA drain; the buffer and clock lease remain quarantined. */
int Mt8167TransportStop(Mt8167Transport *t);
#endif
#endif
