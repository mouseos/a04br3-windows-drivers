/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_DRAW_ABI_H
#define GEO_DRAW_ABI_H
#include "../clear_protocol.h"
/* Separate opt-in extension: existing packed-clear owner ops v2 is unchanged.
 * Kernel source-linked only. No user pointers, physical/GPU addresses, opaque
 * arbitrary command buffers, or fixture-selector values in this descriptor. */
#define GEO_DRAW_ABI_VERSION 1u
#define GEO_DRAW_MAGIC 0x31444f47u
#define GEO_SHADER_D3D9_SM2 1u
#define GEO_SHADER_D3D10_TOKENS 2u
#define GEO_DRAW_TRIANGLE_LIST 1u
#define GEO_DRAW_MAX_BLOB (256u*1024u)
#define GEO_DRAW_MAX_CODE (64u*1024u)
#define GEO_DRAW_MAX_TOKENS 8192u
struct geo_draw_span { uint32_t offset,bytes; };
struct geo_draw_shader {
 uint32_t format,stage; /* stage: 0 vertex, 1 pixel, independent of D3D token IDs */
 struct geo_draw_span tokens,usc,constants;
 uint32_t temps,shareds,vtxins,vtxouts,input_mask,constant_mask;
};
struct geo_draw_state {
 uint32_t topology,first_vertex,vertex_count,vertex_stride,vertex_offset;
 uint32_t viewport[6]; /* IEEE binary32 x,y,width,height,minZ,maxZ, bit patterns */
 int32_t scissor[4]; /* left,top,right,bottom */
 uint32_t cull,front_ccw,scissor_enable,blend_enable,color_write_mask;
 uint32_t depth_enable,depth_write,stencil_enable;
};
/* D3D rect maxima are exclusive. Clamp signed coordinates before hardware
 * packing; empty intersections remain empty and never wrap into a full RT. */
static inline int32_t geo_draw_scissor_bounds(const struct geo_draw_state *s,
 uint32_t width,uint32_t height,uint32_t bounds[4]) {
 uint32_t i,extent;
 if(!s||!bounds||!width||!height||width>0x7fffffffu||height>0x7fffffffu||s->scissor_enable>1u)return GEO_INVALID;
 if(!s->scissor_enable){bounds[0]=bounds[1]=0;bounds[2]=width;bounds[3]=height;return GEO_OK;}
 if(s->scissor[0]>s->scissor[2]||s->scissor[1]>s->scissor[3])return GEO_INVALID;
 for(i=0;i<4;i++){extent=(i&1u)?height:width;bounds[i]=s->scissor[i]<0?0u:(uint32_t)s->scissor[i]>extent?extent:(uint32_t)s->scissor[i];}
 return GEO_OK;
}
struct geo_draw_request {
 uint32_t size,version,blob_bytes,reserved;
 /* Private referenced owner allocation IDs resolved by H from VidMm handles.
  * target is the same retained target/MDL as clear; vertex names a distinct
  * actual vertex allocation. No private output ring can substitute target. */
 uint64_t target,vertex;
 struct geo_surface_desc target_desc;
 struct geo_draw_state state;
 struct geo_draw_shader vs,ps;
};
/* User wire representation: IDs MUST be zero; KMD substitutes private owner
 * IDs only after resolving both real VidMm allocation-list handles. */
struct geo_draw_packet {
 uint32_t magic,size,version,target_index,vertex_index,reserved;
 struct geo_draw_request request;
 /* request.blob_bytes bytes immediately follow this fixed header. */
};
#define GEO_DRAW_COMMAND_MAX (GEO_DRAW_MAX_BLOB+sizeof(struct geo_draw_packet))
/* Validate framing/state/ranges only. Success does NOT validate USC ISA,
 * pipeline support, residency or create a native job/capability claim.
 * Call on an H-owned immutable kernel capture, never directly on user memory. */
#ifdef __cplusplus
extern "C"
#endif
int32_t geo_draw_validate_capture(const struct geo_draw_request *,const void *blob,uint32_t blob_bytes);
struct geo_native_draw_ops_v1 {
 uint32_t size,version;
 void *context;
 /* PASSIVE, immutable capture valid for duration of call. E owns actual USC
  * ISA/metadata validation, PDS/PPP/PBE construction, same-MDL resource refs,
  * cache visibility and an independent copy of every consumed byte.
  * No GPU publish here. Failure must leave job=0 unless poisoned job is pinned.
  * submit/IRQ/generation/visibility reuse real owner control-v2, not polling. */
 int32_t (*prepare_draw)(void *,const struct geo_draw_request *,const void *,uint32_t,uint64_t *job);
};
/* E supplies this source-linked optional extension only when real prepare is
 * available. A missing implementation remains a final-link boundary; no stub
 * is provided and the existing CreateV3 signature is not changed. */
struct geo_native_binding;
#ifdef __cplusplus
extern "C"
#endif
int32_t GeoNativeOwnerGetDrawOpsV1(struct geo_native_binding *,struct geo_native_draw_ops_v1 *);
#endif
