#ifndef MT8167_WAVERT_MINIPORT_H
#define MT8167_WAVERT_MINIPORT_H
#include <portcls.h>
#include "stream_backend.h"
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
/* PASSIVE_LEVEL. Software diagnostic only; stream RUN is rejected. */
NTSTATUS Mt8167NewMiniport(PMINIPORTWAVERT *out);
/* Diagnostic integration seam. Current PnP adapter never supplies a backend. */
NTSTATUS Mt8167NewMiniportWithBackend(PMINIPORTWAVERT *out, const Mt8167StreamBackend *backend);
/* Actual adapter speaker only: CT3 nominal timing, not measured qualification.
 * Separate factory prevents HP/capture/HFP from inheriting the speaker model. */
NTSTATUS Mt8167NewSpeakerMiniportWithBackend(PMINIPORTWAVERT *out, const Mt8167StreamBackend *backend);
#endif
#endif
