#ifndef MT8167_WAVERT_MINIPORT_H
#define MT8167_WAVERT_MINIPORT_H
#include <portcls.h>
#include "stream_backend.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
/* PASSIVE_LEVEL. Standalone diagnostic factory has no hardware backend. */
NTSTATUS Mt8167NewMiniport(PMINIPORTWAVERT *out);
/* PnP integration: referenced backend owns actual DMA/IRQ and shared-parent lease. */
NTSTATUS Mt8167NewMiniportWithBackend(PMINIPORTWAVERT *out, const Mt8167StreamBackend *backend);
/* Actual adapter speaker only: CT3 nominal timing, not measured qualification.
 * Separate factory prevents HP/capture/HFP from inheriting the speaker model. */
NTSTATUS Mt8167NewSpeakerMiniportWithBackend(PMINIPORTWAVERT *out, const Mt8167StreamBackend *backend);
#endif
#endif
