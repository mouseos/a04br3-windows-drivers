/* SPDX-License-Identifier: MIT */
#ifndef GEO_UMD_TOKENIZED_SHADER_H
#define GEO_UMD_TOKENIZED_SHADER_H
#include <windows.h>
#include <winternl.h>
#include <d3d10umddi.h>
#include "../compiler/shader_compiler.h"
/* Typed frontend owns the compiler and serializes compilation. Compiler/PCO
 * context must outlive all returned objects. Tokens/signatures survive Set/
 * Delete until the real frontend retires its references, then release here. */
struct geo_umd_tokenized_shader {
 struct geo_shader_compiler *compiler;
 uint32_t *tokens,token_bytes;
 struct geo_sm4_signatures signatures;
 struct geo_compiled_shader compiled;
};
HRESULT geo_umd_compile_shader10(struct geo_shader_compiler *,uint32_t stage,const UINT *,
 const D3D10DDIARG_STAGE_IO_SIGNATURES *,struct geo_umd_tokenized_shader **);
HRESULT geo_umd_compile_shader11(struct geo_shader_compiler *,uint32_t stage,const UINT *,
 const D3D11_1DDIARG_STAGE_IO_SIGNATURES *,struct geo_umd_tokenized_shader **);
void geo_umd_release_tokenized_shader(struct geo_umd_tokenized_shader *);
#endif
