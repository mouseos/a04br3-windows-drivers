/* SPDX-License-Identifier: MIT */
#ifndef GEO_PAIR_COMPILER_H
#define GEO_PAIR_COMPILER_H
#include "shader_compiler.h"
#include "../frontend/draw_link_abi.h"
struct geo_compiled_pair {struct geo_compiled_shader vs,ps;struct geo_draw_pair_link link;};
/* Finite COLOR4 profile. No generic SM4 or feature-level qualification. */
int32_t geo_pco_pair_validate_stage(uint32_t,const uint32_t *,uint32_t,const struct geo_sm4_signatures *);
int32_t geo_pco_compile_pair_blend(void *,const uint32_t *,uint32_t,const struct geo_sm4_signatures *,const uint32_t *,uint32_t,const struct geo_sm4_signatures *,struct geo_compiled_pair *,uint32_t);
int32_t geo_pco_compile_pair(void *,const uint32_t *,uint32_t,const struct geo_sm4_signatures *,
 const uint32_t *,uint32_t,const struct geo_sm4_signatures *,struct geo_compiled_pair *);
#endif
