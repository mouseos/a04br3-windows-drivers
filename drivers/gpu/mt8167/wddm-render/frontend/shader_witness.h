/* SPDX-License-Identifier: MIT */
#ifndef GEO_SHADER_WITNESS_H
#define GEO_SHADER_WITNESS_H
#include <stdint.h>
#define GEO_SHADER_WITNESS_MAGIC 0x31575347u
#define GEO_SHADER_WITNESS_VERSION 1u
#define GEO_SHADER_WITNESS_ISA 0x00033001u
#define GEO_SHADER_WITNESS_MAX_INSNS 128u
#define GEO_SHADER_WITNESS_MAX_CODE 512u
/* Canonical scalar fields; no PCO pointers, C bitfield layout, GPU addresses,
 * resource selectors, code-signing keys, or compiler process addresses. */
struct geo_shader_ref {uint32_t value,type,bank,channels,bits,dtype,modifiers;};
struct geo_shader_insn {
 uint32_t opcode,num_dests,num_srcs;
 struct geo_shader_ref dest[2],src[3];
 uint32_t modifiers[19];
};
struct geo_shader_witness {
 uint32_t magic,version,isa,stage,count,reserved;
 /* count records follow. */
};
struct geo_shader_metadata {uint32_t stage,temps,shareds,vtxins,vtxouts,input_mask,constant_mask;};
/* Immutable capture only. Re-encode every byte with reviewed source encoders.
 * Admits no memory op, texture, branch, predication, indirect register or spill.
 * The shader's arithmetic semantics are compiler responsibility; native checks
 * executable safety/register/resource bounds, not an exact-program allowlist. */
int32_t geo_shader_witness_measure(const void *,uint32_t,uint32_t *,uint32_t *);
int32_t geo_shader_witness_validate(const void *,uint32_t,const void *,uint32_t,const struct geo_shader_metadata *);
#endif
