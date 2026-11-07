/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#ifndef GEO_DRAW_LINK_ABI_H
#define GEO_DRAW_LINK_ABI_H
#include "draw_abi.h"
/* ABI1 remains byte-for-byte unchanged. This is a separate immutable,
 * source-linked pair snapshot, never raw PCO structs or hardware words.
 * Version2 is not admitted by the ABI1 prepare callback. */
#define GEO_DRAW_LINK_ABI_VERSION 2u
#define GEO_DRAW_LINK_MAX_VARYINGS 1u
#define GEO_DRAW_SEMANTIC_COLOR 1u
#define GEO_DRAW_INTERP_SMOOTH 1u
#define GEO_DRAW_INTERP_NOPERSPECTIVE 2u
#define GEO_DRAW_INTERP_FLAT 3u
struct geo_draw_link_range { uint32_t start,count,offset; };
struct geo_draw_link_varying {
 uint32_t semantic,index,components,bit_size,interpolation;
 struct geo_draw_link_range vs_output,fs_coeff;
};
struct geo_draw_pair_link {
 uint32_t size,version,varying_count,reserved;
 uint32_t vs_vtxouts,vs_f32_smooth,vs_f32_flat,vs_f32_npc;
 uint32_t vs_f16_smooth,vs_f16_flat,vs_f16_npc;
 struct geo_draw_link_range position;
 uint32_t fs_coeffs,fs_uses_z,fs_uses_w;
 uint32_t vs_entry_offset,fs_entry_offset;
 struct geo_draw_link_varying varying[GEO_DRAW_LINK_MAX_VARYINGS];
};
struct geo_draw_request_v2 {
 /* base.size=sizeof(request_v2), base.version=2. All spans relative to the
  * immutable blob, all other shader/state/allocation fields keep ABI1 units.
  * temps/shareds/vtxins/vtxouts come from FINAL linked PCO common/stage data.
  * entry_offset is a BYTE offset in the corresponding USC span. */
 struct geo_draw_request base;
 struct geo_draw_pair_link link;
};
#endif
