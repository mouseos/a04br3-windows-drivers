/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_TEXTURE_DRAW_ABI_H
#define GEO_TEXTURE_DRAW_ABI_H
#include "draw_abi.h"
#define GEO_DRAW_TEXTURE_ABI_VERSION 3u
/* New private storage kind/version; old owner/UMD peers must reject it. */
#define GEO_TEXTURE_POINT_CLAMP 1u
/* Layout3 remains stable; this explicit sampler semantic version cannot be
 * accepted by a POINT-only326 peer. Adapter contract4 negotiates it. */
#define GEO_TEXTURE_FILTER_PROFILE4 0x04000000u
int geo_texture_sampler_profile_valid(uint32_t);
struct geo_draw_request_v3 {
 struct geo_draw_request base;
 uint64_t texture; /* H resolves the third VidMm handle to an owned ID */
 struct geo_surface_desc texture_desc;
 uint32_t sampler_profile,reserved;
};
struct geo_draw_packet_v3 {
 uint32_t magic,size,version,target_index,vertex_index,reserved;
 struct geo_draw_request_v3 request;
 uint32_t texture_index,reserved_texture;
};
#define GEO_DRAW_TEXTURE_COMMAND_MAX (GEO_DRAW_MAX_BLOB+sizeof(struct geo_draw_packet_v3))
int32_t geo_draw_validate_capture_v3(const struct geo_draw_request_v3 *,const void *,uint32_t);
struct geo_native_draw_ops_v3 {
 uint32_t size,version;void *context;
 int32_t (*prepare_draw)(void *,const struct geo_draw_request_v3 *,const void *,uint32_t,uint64_t *);
};
int32_t GeoNativeOwnerGetDrawOpsV3(struct geo_native_binding *,struct geo_native_draw_ops_v3 *);
#endif
