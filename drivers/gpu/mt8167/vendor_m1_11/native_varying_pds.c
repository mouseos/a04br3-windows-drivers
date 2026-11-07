/*
 * Copyright © 2022 Imagination Technologies Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* SPDX-License-Identifier: MIT
 * CPU encoding derived from Mesa Imagination source (MIT):
 * pvr_arch_pipeline.c:pvr_graphics_pipeline_setup_fragment_coeff_program,
 * pvr_arch_cmd_buffer.c:pvr_setup_output_selects / fragment PDS state,
 * pds/pvr_pds.c:pvr_pds_coefficient_loading and pvr_rogue_pds_{defs,encode}.h.
 * Fixed source and units documented in NATIVE_VARYING_ABI2.md.
 */
#include "native_varying_pds.h"
static void issue(struct a04_m111_varying_pds *m,uint32_t src,uint32_t dest)
{
 uint32_t i=m->code_dwords++;
 uint64_t word=(uint64_t)src|((uint64_t)dest<<54);
 m->data[2*i]=(uint32_t)word;m->data[2*i+1]=(uint32_t)(word>>32);
 m->code[i]=0xf0000004u|(i<<8); /* DOUTI, cc0, constant64 index i */
}
int32_t a04_m111_build_varying_pds(struct a04_m111_varying_pds *out,
 const struct geo_draw_pair_link *r)
{
 struct a04_m111_varying_pds m={{0},{0},0,0,0,0,0,0,0,0};
 const struct geo_draw_link_varying *v;
 uint32_t zw,offset,src;
 if(!out || !r || r->size!=sizeof(*r) || r->version!=2 || r->reserved ||
    r->varying_count!=1 || r->fs_uses_z>1 || r->fs_uses_w>1)return GEO_INVALID;
 v=&r->varying[0];zw=r->fs_uses_z+r->fs_uses_w;
 /* First profile is exactly position4 + color4. Preserve original range
  * offsets as metadata but do not add them to hardware component indices:
  * the Mesa iterator consumes pco_range.start/count, not offset. */
 if(r->vs_vtxouts!=8 || r->position.start || r->position.count!=4 ||
    r->vs_f16_smooth || r->vs_f16_flat || r->vs_f16_npc ||
    v->semantic!=GEO_DRAW_SEMANTIC_COLOR || v->index || v->components!=4 ||
    v->bit_size!=32 || v->vs_output.start!=4 || v->vs_output.count!=4 ||
    v->fs_coeff.start!=zw*4 || v->fs_coeff.count!=16 || r->fs_coeffs!=zw*4+16)
    return GEO_UNSUPPORTED;
 if(v->interpolation==GEO_DRAW_INTERP_SMOOTH) {
    if(r->vs_f32_smooth!=4 || r->vs_f32_flat || r->vs_f32_npc)return GEO_INVALID;
 } else if(v->interpolation==GEO_DRAW_INTERP_NOPERSPECTIVE) {
    if(r->vs_f32_npc!=4 || r->vs_f32_flat || r->vs_f32_smooth)return GEO_INVALID;
 } else return GEO_UNSUPPORTED; /* Flat provoking vertex not selected here. */
 if(r->fs_uses_z) {
    offset=r->fs_uses_w*2;
    issue(&m,0x03000000u|(offset<<8)|offset,0);
 }
 if(r->fs_uses_w)issue(&m,0x03000000u,r->fs_uses_z);
 offset=(v->vs_output.start-2-(1-r->fs_uses_z)-(1-r->fs_uses_w))*2;
 src=0x03000000u|(3u<<18)|(offset<<8)|offset;
 if(v->interpolation==GEO_DRAW_INTERP_SMOOTH)src|=1u<<16;
 issue(&m,src,v->fs_coeff.start/4);
 m.data[2*m.code_dwords-1]|=0x80000000u; /* last ISSUE */
 m.code[m.code_dwords-1]|=1u<<26; /* last END */
 m.data_dwords=(2*m.code_dwords+3)&~3u;m.temps=1;
 m.output_select=(r->vs_vtxouts<<24)|(r->fs_uses_z<<21)|(r->fs_uses_w<<16);
 m.varying0=r->vs_f32_smooth|(r->vs_f32_flat<<8)|(r->vs_f32_npc<<16);
 m.pds_varying_size=m.data_dwords/4;
 m.usc_varying_size=(r->fs_coeffs+15)/16;
 *out=m;return GEO_OK;
}
