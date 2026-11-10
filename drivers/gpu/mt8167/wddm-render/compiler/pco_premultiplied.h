/* SPDX-License-Identifier: MIT */
#ifndef GEO_PCO_PREMULTIPLIED_H
#define GEO_PCO_PREMULTIPLIED_H
#include "pco/pco.h"
#include "pco/pco_data.h"
/* Mesa nir_lower_blend clamps normalized-format source inputs before blend.
 * RGBA source, BGRA8 PIXOUT destination: ONE + INV_SRC_ALPHA for RGB and A.
 * pco_trans_nir.c:trans_load_output_fs reads PIXOUT with OLCHK. The consumer
 * must also select the framebuffer-fetch TRANSLUCENT ISP pass and load the
 * existing RT contents into the tile. This helper alone is not that contract. */
static inline nir_def *geo_pco_premultiplied_bgra(nir_builder *b,pco_data *data,nir_def *rgba)
{
 unsigned bgra[4]={2,1,0,3};
 nir_def *src=nir_swizzle(b,nir_fsat(b,rgba),bgra,4);
 nir_def *packed=nir_load_output(b,1,32,nir_imm_int(b,0),.base=0,
  .dest_type=nir_type_invalid|32,.io_semantics={.location=FRAG_RESULT_DATA0,.num_slots=1});
 nir_def *dst=nir_unpack_unorm_4x8(b,packed);
 nir_def *inverse=nir_fsub(b,nir_imm_float(b,1.0f),nir_channel(b,src,3));
 data->fs.outputs[FRAG_RESULT_DATA0]=(pco_range){.start=0,.count=1};
 data->fs.roa_color=true;data->fs.uses.fbfetch=true;data->fs.uses.olchk_skip=false;
 b->shader->info.fs.uses_fbfetch_output=true;
 return nir_pack_unorm_4x8(b,nir_fadd(b,src,nir_fmul(b,dst,inverse)));
}
#endif
