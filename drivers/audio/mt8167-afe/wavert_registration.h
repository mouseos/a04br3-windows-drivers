#ifndef MT8167_WAVERT_REGISTRATION_H
#define MT8167_WAVERT_REGISTRATION_H
#include <portcls.h>
/* Experimental build only. Caller must have completed PnP resource arbitration,
 * power/clock ownership and teardown setup before registering a subdevice.
 * There is presently no caller and no miniport implementation. */
#if defined(A04_MT8167_WAVERT_EXPERIMENTAL) && A04_MT8167_WAVERT_EXPERIMENTAL
NTSTATUS Mt8167RegisterWaveRt(PDEVICE_OBJECT device, PIRP irp,
                              PRESOURCELIST resources, PMINIPORTWAVERT miniport);
#endif
#endif

#if defined(A04_AUDIO_CAPTURE) && A04_AUDIO_CAPTURE
NTSTATUS Mt8167RegisterCaptureWaveRt(PDEVICE_OBJECT,PIRP,PRESOURCELIST,PMINIPORTWAVERT);
#endif
