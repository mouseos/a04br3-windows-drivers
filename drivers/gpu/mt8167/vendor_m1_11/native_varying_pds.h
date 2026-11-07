/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_VARYING_PDS_H
#define A04_M111_NATIVE_VARYING_PDS_H
#include "../wddm-render/frontend/draw_link_abi.h"
#include "native_triangle_ppp.h"
/* Up to Z, W and one COLOR4 iterator. PDS data is 128-bit aligned; code
 * immediately follows data. These are CPU results, not device addresses. */
struct a04_m111_varying_pds {
 uint32_t data[8],code[3];
 uint32_t data_dwords,code_dwords,temps;
 uint32_t output_select,varying0,varying1;
 uint32_t pds_varying_size,usc_varying_size;
};
/* Produces real DOUTI instructions and PPP scalar fields only. Does NOT
 * validate USC ISA, publish a job, or qualify runtime/caps. Failure is atomic. */
int32_t a04_m111_build_varying_pds(struct a04_m111_varying_pds *,
 const struct geo_draw_pair_link *);
/* Complete fixed 800x600 draw PPP, with linked coefficient program instead
 * of ABI1's no-varying state. All addresses are current owner PDS offsets.
 * uniform_dwords is the actual PDS uniform data extent, not shader constant
 * backing capacity. USC shareds/temp counts come from final linked metadata. */
int32_t a04_m111_build_linked_triangle_ppp(void *out,size_t bytes,
 const struct a04_m111_triangle_ppp_input *,const struct geo_draw_pair_link *,
 uint32_t uniform_dwords,uint32_t fragment_pds_temps,uint32_t fs_shareds);
#endif
