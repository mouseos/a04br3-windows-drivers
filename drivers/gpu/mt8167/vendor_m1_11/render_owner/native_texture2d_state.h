/* SPDX-License-Identifier: MIT */
#ifndef A04_NATIVE_TEXTURE2D_STATE_H
#define A04_NATIVE_TEXTURE2D_STATE_H
#include <stdint.h>
#include "../../wddm-render/texture_layout.h"
/* Internal producer contract, not a user wire ABI or capability declaration.
 * Owned retained backing is supplied by the caller, never an unchecked DDI VA.
 * Single-mip/single-layer BGRA8/RGBA8, normalized regular filters, no comparison.
 * Dimension/stride field widths are validated independently of residency budgets. */
#define A04_TEXTURE2D_STATE_VERSION 1u
#define A04_TEXTURE2D_BGRA8 87u
#define A04_TEXTURE2D_RGBA8 28u
#define A04_TEXTURE2D_MAX_EDGE GEO_TEXTURE_MAX_EDGE
#define A04_TEXTURE2D_MAX_BYTES GEO_TEXTURE_MAX_BYTES
struct a04_texture2d_storage {
 uint32_t size,version,width,height,row_pitch,bytes,format;
 uint32_t mip_levels,array_size,sample_count,reserved;
 uint64_t gpu;
};
struct a04_texture2d_state {uint32_t sampler[4],image[4];};
/* Error leaves output unchanged. This proves descriptor layout/bounds only;
 * it does not acquire a backing lease or submit GPU work. */
int32_t a04_m111_texture2d_state(struct a04_texture2d_state *,const struct a04_texture2d_storage *);
int32_t a04_m111_texture2d_state_filtered(struct a04_texture2d_state *,const struct a04_texture2d_storage *,uint32_t filter,uint32_t max_anisotropy);
#endif
