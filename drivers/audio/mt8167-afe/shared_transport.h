#ifndef MT8167_SHARED_TRANSPORT_H
#define MT8167_SHARED_TRANSPORT_H
#include "shared_audio.h"
#include "stream_transport.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
/* PASSIVE_LEVEL; caller holds owner lifetime and excludes PnP stop/removal.
 * The parent lease is the sole clock/codec owner for this path.
 * No production PnP caller or provider exists. */
int Mt8167SharedTransportStart(struct mt8167_audio_shared_client *,Mt8167Transport *,uint32_t);
#endif
#endif
