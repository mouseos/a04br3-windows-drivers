/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT6630_HFP_TOPOLOGY_H
#define MT6630_HFP_TOPOLOGY_H
#include "ks_endpoints.h"
/* topology pins: input0 -> output1; render external1, capture external0.
 * Runtime HFP categories/container, no manufactured jack connection/volume. */
NTSTATUS Mt6630NewHfpTopology(struct a04_bt_hfp_session *,ULONG,
 const struct a04_bt_hfp_metadata *,PMINIPORTTOPOLOGY *);
#endif
