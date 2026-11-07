/* SPDX-License-Identifier: MIT */
#ifndef A04_M111_NATIVE_DRAW_JOB_H
#define A04_M111_NATIVE_DRAW_JOB_H
#include "native_triangle_suite.h"
#include "../wddm-render/frontend/draw_abi.h"
struct a04_m111_draw_job {
 uint8_t ta[A04_M111_TA_BYTES],three[A04_M111_3D_BYTES];
 uint8_t data[0x540],usc[0x300],pds[0x380];
};
#define A04_M111_DRAW_SCRATCH (sizeof(struct a04_m111_draw_job)+2u*sizeof(struct a04_m111_triangle_suite)+16u)
/* Finite profile: full800x600 viewport, one float4 clip-space triangle,
 * H source-PCO position passthrough VS24 + BGRA constant-color PS64.
 * Same-target nonnormalized nearest background LOAD preserves existing RT.
 * Every pointer/descriptor/task is synthesized from current owner mappings.
 * No arbitrary ISA admission. GPU execution/visibility is separate. */
int32_t a04_m111_build_draw_job(struct a04_m111_draw_job *,
 const struct a04_m111_clear_job_input *,uint64_t vertex_gpu,
 const struct geo_draw_request *,const void *blob,uint32_t bytes,void *scratch,size_t);
#endif
