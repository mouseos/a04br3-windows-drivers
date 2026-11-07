/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_ADAPTER_INFO_H
#define GEO_ADAPTER_INFO_H
#include "aperture.h"
#define GEO_ADAPTER_INFO_MAGIC 0x31494f47u
/* QueryAdapterInfo UMDRIVERPRIVATE snapshot. This is memory/protocol info,
 * not shader caps or a D3D feature-level declaration. No user GPU payload. */
struct geo_adapter_memory_info {
 uint32_t size,version,magic,owner_ops_version,control_version,format,max_allocation_bytes,reserved;
 struct geo_aperture_config aperture;
};
#endif
