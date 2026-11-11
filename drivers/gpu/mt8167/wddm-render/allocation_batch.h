/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_ALLOCATION_BATCH_H
#define GEO_ALLOCATION_BATCH_H
#include "owner_abi.h"
/* SYS-private source-linked helpers, NOT additions to the public owner ops,
 * wire contract or adapter layout. PASSIVE, caller holds frontend session. */
#define GEO_ALLOCATION_BATCH_MAX 256u
NTSTATUS GeoNativeCreateAllocationBatch(const struct geo_owner_ops *,UINT,
 const struct geo_surface_desc *,uint64_t *);
NTSTATUS GeoNativeDestroyAllocationBatch(const struct geo_owner_ops *,UINT,const uint64_t *);
#endif
