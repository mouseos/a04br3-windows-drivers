/* SPDX-License-Identifier: MIT */
#ifndef GEO_TEXTURE2D_SM4_H
#define GEO_TEXTURE2D_SM4_H
#include "shader_compiler.h"
/* Separate opt-in compiler contract. Default arithmetic compiler remains
 * unchanged and rejects resource operations. No driver-wire ABI change. */
#define GEO_TEXTURE2D_COMPILER_VERSION 1u
struct geo_texture2d_program {uint32_t coordinate_constant,coordinate_x,coordinate_y;};
/* PS4.0, one float4 CB0 vector supplying UV, t0 float2D/s0 normal POINT+CLAMP,
 * SAMPLE -> o0.xyzw -> RET. No offsets/LOD/comparison/arrays/MSAA/flow. */
int32_t geo_sm4_texture2d_decode(const uint32_t *,uint32_t,const struct geo_sm4_signatures *,struct geo_texture2d_program *);
/* Shared register contract: sampler0..3, image4..7, CB0 vectors from8.
 * Native may call this code only after negotiating an owned texture job ABI;
 * these fields must not be interpreted as the old flat draw constants. */
int32_t geo_pco_compile_texture2d(void *,const uint32_t *,uint32_t,const struct geo_sm4_signatures *,struct geo_compiled_shader *,struct geo_texture2d_program *);
#endif
