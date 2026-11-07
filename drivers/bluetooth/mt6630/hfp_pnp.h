/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef MT6630_HFP_PNP_H
#define MT6630_HFP_PNP_H
#include "hfp_topology.h"
struct mt6630_hfp_pnp;
/* Audio PortCls FDO only, never the Bluetooth WDF FDO. PASSIVE_LEVEL.
 * Owner must reserve four PortCls subdevices, provide INF interface entries,
 * resources and provider v2. Start success means notification registration,
 * NOT endpoint enumeration, HFP connection or DMA success.
 * Stop before D0 resource/module release. On stop failure retain this handle,
 * FDO/resources/provider until actual owner rundown; no automatic retry.
 * On start cleanup failure *out is retained even though status failed.
 */
NTSTATUS Mt6630HfpPnpStart(PDEVICE_OBJECT,PRESOURCELIST,
 const struct a04_bt_hfp_provider *,struct mt6630_hfp_pnp **out);
NTSTATUS Mt6630HfpPnpStop(struct mt6630_hfp_pnp *);
#endif
