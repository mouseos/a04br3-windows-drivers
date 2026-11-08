/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT8167_HEADPHONE_ENDPOINT_H
#define MT8167_HEADPHONE_ENDPOINT_H
#include "stream_backend.h"
#include "../../input/mt8167-accdet/socket.h"
/* Internal, referenced, actual parent-owned provider, not a user profile.
 * No current provider is fabricated. The shared DL1 owner must enforce one
 * stream across speaker/HP, switch the real analog route under that lease,
 * preserve capture, retain ambiguous DMA resources and revoke on PnP/power.
 * qualified checks actual stereo calibration generation, deadline, sense
 * qualification and the implemented HP route/restore contract; not just CM. */
struct Mt8167HeadphoneProvider {
 Mt8167StreamBackend stream;
 NTSTATUS (*qualified)(void *);
 NTSTATUS (*readJack)(void *,struct mt8167_accdet_socket *);
 /* Must create an actual HP WaveRT miniport with verified format, DMA,
  * stop/revoke and hardware-latency behavior. Do not reuse a speaker's zero
  * latency as a measurement or set capture=TRUE to smuggle HP latency. */
 NTSTATUS (*newWaveMiniport)(void *,PMINIPORTWAVERT *);
};
/* Call during StartDevice only. Requires a real HP backend before any
 * subdevice registration. NOT_READY is not a fabricated successful endpoint.
 * After partial registration any failure requires normal PortCls StartDevice
 * unwind; do not continue with a half-registered graph. */
extern "C" NTSTATUS Mt8167RegisterHeadphoneWaveRt(PDEVICE_OBJECT,PIRP,PRESOURCELIST,
 const Mt8167HeadphoneProvider *);
#endif
