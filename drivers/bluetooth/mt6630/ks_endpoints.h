/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef A04_BT_KS_ENDPOINTS_H
#define A04_BT_KS_ENDPOINTS_H
#include <portcls.h>
#include "hfp_endpoint.h"
/* PASSIVE_LEVEL factory for a PortCls adapter owned by the real audio driver.
 * direction=0 speaker,1 microphone; streaming pin0, bridge pin1. This does not
 * register subdevices on the Bluetooth WDF FDO or fabricate an audio endpoint.
 * Adapter must install topology/physical connections and remove both filters
 * before HFP session detach. There is no dummy/default provider. */
NTSTATUS Mt6630NewHfpMiniport(struct a04_bt_hfp_session *,ULONG direction,PMINIPORTWAVERT *);
#endif
