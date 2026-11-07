/* SPDX-License-Identifier: MIT */
#ifndef GEO_UMD_SHADER_STATE_H
#define GEO_UMD_SHADER_STATE_H
#include "clear_ddi.h"
#include "../compiler/shader_compiler.h"
/* Source-linked real frontend owns initialization/teardown and draw snapshot.
 * This installs only implemented shader DDIs and makes no caps claim. */
HRESULT geo_umd_shader_init(struct geo_umd_device *,const struct geo_shader_compiler *);
void geo_umd_install_shader_ddis(D3DDDI_DEVICEFUNCS *);
HRESULT geo_umd_shader_shutdown(struct geo_umd_device *);
/* Borrowed until next state mutation. Actual frontend must copy semantic
 * shader/constants into its validated kernel Render request before returning
 * from Flush. Do not pass this UMD pointer or unchecked USC to the owner. */
HRESULT geo_umd_shader_snapshot(struct geo_umd_device *,UINT stage,const struct geo_compiled_shader **,const UINT **constants,UINT *valid_mask);
HRESULT geo_umd_shader_tokens(struct geo_umd_device *,UINT stage,const UINT **tokens,UINT *bytes);
#endif
