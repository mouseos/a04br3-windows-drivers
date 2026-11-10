/* SPDX-License-Identifier: MIT. Real NIR/PCO sampling, no shader substitution. */
#include "texture2d_sm4.h"
#include "../clear_protocol.h"
#include "pco/pco.h"
#include "pco/pco_data.h"
#include "pco_premultiplied.h"
#include "pco/pco_internal.h"
#include "util/ralloc.h"
#include <string.h>
int32_t geo_pco_compile_texture2d_blend(void *context,const uint32_t *tokens,uint32_t bytes,
 const struct geo_sm4_signatures *signature,struct geo_compiled_shader *out,struct geo_texture2d_program *binding,uint32_t blend)
{
 pco_ctx *ctx=context;pco_shader *shader;nir_builder b;pco_data data={0};pco_smp_params params={0};
 struct geo_texture2d_program program;nir_def *coordinates,*pixel;int32_t status;unsigned bgra[4]={2,1,0,3};
 if(!ctx||!out||!binding||blend>1)return GEO_INVALID;
 memset(out,0,sizeof(*out));status=geo_sm4_texture2d_decode(tokens,bytes,signature,&program);if(status)return status;
 b=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,pco_nir_options(),"D3D-SM4-source-Texture2D");
 coordinates=nir_load_preamble(&b,4,32,.base=8+4*program.coordinate_constant);
 params.smp_state=nir_load_preamble(&b,4,32,.base=0);
 params.tex_state=nir_load_preamble(&b,4,32,.base=4);
 params.coords=nir_vec2(&b,nir_channel(&b,coordinates,program.coordinate_x),nir_channel(&b,coordinates,program.coordinate_y));
 params.dest_type=nir_type_float32;params.sampler_dim=GLSL_SAMPLER_DIM_2D;
 params.nncoords=false;params.sample_components=4;
 /* pco_emit_nir_smp and lower_smp explicitly admit direct preamble state.
  * Normalized2D implicit LOD, native contract restricts image to one mip. */
 pixel=&pco_emit_nir_smp(&b,&params)->def;
 nir_frag_store_pco(&b,blend?geo_pco_premultiplied_bgra(&b,&data,pixel):nir_pack_unorm_4x8(&b,nir_swizzle(&b,pixel,bgra,4)),.base=0);
 nir_jump(&b,nir_jump_return);data.common.shareds=8+4*(program.coordinate_constant+1);
 pco_preprocess_nir(ctx,b.shader,&data);pco_lower_nir(ctx,b.shader,&data);pco_postprocess_nir(ctx,b.shader,&data);
 shader=pco_trans_nir(ctx,b.shader,&data,NULL);
 if(!shader){ralloc_free(b.shader);return GEO_UNSUPPORTED;}
 pco_process_ir(ctx,shader);pco_encode_ir(ctx,shader);
 out->size=sizeof(*out);out->version=1;out->stage=GEO_SM2_PIXEL;
 out->code_bytes=pco_shader_binary_size(shader);out->code=pco_shader_binary_data(shader);out->storage=shader;
 out->temps=pco_shader_data(shader)->common.temps;out->shareds=pco_shader_data(shader)->common.shareds;
 out->constant_mask=1u<<program.coordinate_constant;out->output_format=GEO_FORMAT_A8R8G8B8;
 ralloc_free(b.shader);
 if(!out->code||!out->code_bytes){ralloc_free(shader);memset(out,0,sizeof(*out));return GEO_UNSUPPORTED;}
 *binding=program;return GEO_OK;
}

int32_t geo_pco_compile_texture2d(void *c,const uint32_t *t,uint32_t n,const struct geo_sm4_signatures *s,
 struct geo_compiled_shader *out,struct geo_texture2d_program *b)
{ return geo_pco_compile_texture2d_blend(c,t,n,s,out,b,0); }
