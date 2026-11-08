/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_ADAPTER_INFO_H
#define GEO_ADAPTER_INFO_H
#include "aperture.h"
#define GEO_ADAPTER_INFO_MAGIC 0x31494f47u
#define GEO_COMPILER_BVNC UINT64_C(0x001600280036001e)
/* QueryAdapterInfo UMDRIVERPRIVATE snapshot. This is memory/protocol info,
 * not shader caps or a D3D feature-level declaration. No user GPU payload. */
struct geo_adapter_memory_info {
 uint32_t size,version,magic,owner_ops_version,control_version,format,max_allocation_bytes,texture_draw_version;
 struct geo_aperture_config aperture;
 uint64_t compiler_bvnc; /* selected fixed compiler target, not a register readback */
 uint32_t draw_version,vertex_max_bytes;
};
#endif
