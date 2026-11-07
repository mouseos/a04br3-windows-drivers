/* SPDX-License-Identifier: MIT */
#ifndef GEO_SHADER_COMPILER_H
#define GEO_SHADER_COMPILER_H
#include "sm2.h"
#include "sm4.h"
struct geo_compiled_shader {
 uint32_t size,version,stage,code_bytes,temps,shareds,vtxins,vtxouts;
 uint32_t input_mask,constant_mask,output_format;
 uint32_t input_semantics[16];
 const void *code;void *storage;
};
struct geo_shader_compiler {
 uint32_t size,version;void *context;
 int32_t (*compile)(void *,uint32_t stage,const uint32_t *,uint32_t bytes,struct geo_compiled_shader *,struct geo_sm2_error *);
 void (*release)(void *,struct geo_compiled_shader *);
};
/* Caller provides an actual PCO context and initialized fixed BVNC device
 * info. No host compile executable, arbitrary DLL or shader blob broker. */
int32_t geo_pco_compiler_init(struct geo_shader_compiler *,void *pco_context);
int32_t geo_pco_compile_tokenized(void *,uint32_t stage,const uint32_t *,uint32_t bytes,
 const struct geo_sm4_signatures *,struct geo_compiled_shader *,struct geo_sm2_error *);
void geo_pco_compiler_finish(struct geo_shader_compiler *);
#endif
