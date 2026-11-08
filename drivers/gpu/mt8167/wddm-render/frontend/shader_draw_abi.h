/* SPDX-License-Identifier: MIT */
#ifndef GEO_SHADER_DRAW_ABI_H
#define GEO_SHADER_DRAW_ABI_H
#include "draw_abi.h"
#include "shader_witness.h"
#define GEO_DRAW_SHADER_ABI_VERSION 4u
struct geo_draw_request_v4 {struct geo_draw_request base;struct geo_draw_span witness[2];uint32_t isa,reserved;};
struct geo_draw_packet_v4 {uint32_t magic,size,version,target_index,vertex_index,reserved;struct geo_draw_request_v4 request;};
#define GEO_DRAW_SHADER_COMMAND_MAX (GEO_DRAW_MAX_BLOB+sizeof(struct geo_draw_packet_v4))
int32_t geo_draw_validate_capture_v4(const struct geo_draw_request_v4 *,const void *,uint32_t);
struct geo_native_draw_ops_v4 {uint32_t size,version;void *context;int32_t (*prepare_draw)(void *,const struct geo_draw_request_v4 *,const void *,uint32_t,uint64_t *);};
struct geo_native_binding;
int32_t GeoNativeOwnerGetDrawOpsV4(struct geo_native_binding *,struct geo_native_draw_ops_v4 *);
#endif
