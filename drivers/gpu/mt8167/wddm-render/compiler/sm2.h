/* SPDX-License-Identifier: MIT */
#ifndef GEO_SM2_H
#define GEO_SM2_H
#include <stdint.h>
#define GEO_SM2_MAX_TOKENS 2048u
#define GEO_SM2_MAX_INSNS 128u
#define GEO_SM2_CONSTANTS 32u
#define GEO_SM2_VERTEX 0u
#define GEO_SM2_PIXEL 1u
/* Bounded straight-line SM2 subset, NOT a Shader Model 2 capability claim. */
struct geo_sm2_operand {uint32_t file,index,mask,swizzle,negate,saturate;};
struct geo_sm2_instruction {
 uint32_t opcode;struct geo_sm2_operand dst,src[3];uint32_t immediate[4];
};
struct geo_sm2_program {
 uint32_t stage,count,input_mask,constant_mask,immediate_mask;
 uint32_t input_semantics[16];
 struct geo_sm2_instruction instructions[GEO_SM2_MAX_INSNS];
};
struct geo_sm2_error {uint32_t word,opcode;};
/* Returns 0, INVALID or UNSUPPORTED; never substitutes a fixed shader. */
int32_t geo_sm2_decode(struct geo_sm2_program *,uint32_t stage,const uint32_t *,uint32_t bytes,struct geo_sm2_error *);
#endif
