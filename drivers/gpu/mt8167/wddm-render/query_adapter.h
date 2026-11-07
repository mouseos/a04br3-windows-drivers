/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_QUERY_ADAPTER_H
#define GEO_QUERY_ADAPTER_H
#include "lifecycle.h"
NTSTATUS GeoConfigureAperture(struct geo_kmd_adapter *,const struct geo_aperture_config *);
DXGKDDI_QUERYADAPTERINFO GeoQueryAdapterInfo;
/* Actual kernel caps and legacy packet-boundary scheduler policy only;
 * never a shader/feature-level declaration. */
NTSTATUS GeoBuildRequiredDriverCaps(struct geo_kmd_adapter *,DXGK_DRIVERCAPS *);
#endif
