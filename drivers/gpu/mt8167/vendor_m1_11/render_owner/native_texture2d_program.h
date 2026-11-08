/* SPDX-License-Identifier: MIT
 * Source-generated validation bytes ONLY; never a replacement shader.
 * Actual SM4 -> NIR -> PCO: compiler/pco_texture2d.c, source ca59896a.
 * The native submit path must copy the provided, validated USC bytes.
 */
#ifndef A04_NATIVE_TEXTURE2D_PROGRAM_H
#define A04_NATIVE_TEXTURE2D_PROGRAM_H
#include "../../wddm-render/frontend/draw_abi.h"
int32_t a04_m111_texture2d_program(const struct geo_draw_shader *,const void *,uint32_t);
#endif
