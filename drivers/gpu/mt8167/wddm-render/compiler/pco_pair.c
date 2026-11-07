/* SPDX-License-Identifier: MIT
 * Runtime source compiler, derived from E's native-color-pair.c reference.
 * Link/allocation order follows Mesa pvr_arch_pipeline.c; no prebuilt USC. */
#include "pair_compiler.h"
#include "color_sm4.h"
#include "pco/pco.h"
#include "pco/pco_data.h"
#include "pco/pco_internal.h"
#include "util/ralloc.h"
#include <string.h>
int32_t geo_pco_pair_validate_stage(uint32_t stage,const uint32_t *t,uint32_t bytes,const struct geo_sm4_signatures *s) {
 struct a04_color_moves moves;unsigned i;int decoded;
 if(!s||stage>1)return GEO_INVALID;
 if(s->input_count!=(stage?1u:2u)||s->output_count!=(stage?1u:2u))return GEO_UNSUPPORTED;
 for(i=0;i<s->input_count;i++)if(s->inputs[i].reg!=i||s->inputs[i].mask!=15||s->inputs[i].system_value)return GEO_UNSUPPORTED;
 for(i=0;i<s->output_count;i++)if(s->outputs[i].reg!=i||s->outputs[i].mask!=15||s->outputs[i].system_value!=(!stage&&!i?1u:0u))return GEO_UNSUPPORTED;
 decoded=a04_decode_color_sm4(&moves,stage,t,bytes);return decoded==0?GEO_OK:(decoded==1?GEO_INVALID:GEO_UNSUPPORTED);
}
static nir_variable *pair_io(nir_shader *s,nir_variable_mode mode,int loc,const char *name) {
 nir_variable *v=nir_variable_create(s,mode,glsl_vec4_type(),name);
 if(v){v->data.location=loc;v->data.interpolation=INTERP_MODE_SMOOTH;}return v;
}
static int encode_pair_stage(pco_ctx *ctx,nir_shader *nir,pco_data *data,struct geo_compiled_shader *out) {
 pco_shader *s=pco_trans_nir(ctx,nir,data,NULL);const pco_data *d;
 if(!s)return GEO_UNSUPPORTED;
 pco_process_ir(ctx,s);pco_encode_ir(ctx,s);d=pco_shader_data(s);
 out->size=sizeof(*out);out->version=1;out->stage=nir->info.stage==MESA_SHADER_FRAGMENT;
 out->code=pco_shader_binary_data(s);out->code_bytes=pco_shader_binary_size(s);out->storage=s;
 out->temps=d->common.temps;out->shareds=d->common.shareds;out->vtxins=d->common.vtxins;
 out->vtxouts=out->stage?0:d->vs.vtxouts;out->input_mask=out->stage?1:3;
 out->output_format=out->stage?GEO_FORMAT_A8R8G8B8:0;
 return out->code&&out->code_bytes?GEO_OK:GEO_UNSUPPORTED;
}
static struct geo_draw_link_range snapshot_range(pco_range r) {
 struct geo_draw_link_range out={r.start,r.count,r.offset};return out;
}
int32_t geo_pco_compile_pair(void *context,const uint32_t *vt,uint32_t vb,const struct geo_sm4_signatures *vsig,
 const uint32_t *ft,uint32_t fb,const struct geo_sm4_signatures *fsig,struct geo_compiled_pair *out) {
 pco_ctx *ctx=context;struct a04_color_moves moves,fragment;struct geo_compiled_pair made={0};
 pco_data vd={0},fd={0};nir_builder v,f;nir_variable *position,*color,*input;int32_t status;unsigned bgra[4]={2,1,0,3};
 const pco_data *vs,*fs;unsigned components;
 if(!ctx||!out)return GEO_INVALID;
 memset(out,0,sizeof(*out));
 status=geo_pco_pair_validate_stage(0,vt,vb,vsig);if(status)return status;
 status=geo_pco_pair_validate_stage(1,ft,fb,fsig);if(status)return status;
 if(a04_decode_color_sm4(&moves,0,vt,vb)||a04_decode_color_sm4(&fragment,1,ft,fb))return GEO_INVALID;
 v=nir_builder_init_simple_shader(MESA_SHADER_VERTEX,pco_nir_options(),"position-color-pair-vs");
 f=nir_builder_init_simple_shader(MESA_SHADER_FRAGMENT,pco_nir_options(),"position-color-pair-fs");
 if(!v.shader||!f.shader){ralloc_free(v.shader);ralloc_free(f.shader);return GEO_BUSY;}
 position=pair_io(v.shader,nir_var_shader_out,VARYING_SLOT_POS,"position");
 color=pair_io(v.shader,nir_var_shader_out,VARYING_SLOT_VAR0,"color");
 input=pair_io(f.shader,nir_var_shader_in,VARYING_SLOT_VAR0,"color");
 if(!position||!color||!input){status=GEO_BUSY;goto done;}
 nir_store_var(&v,position,nir_load_vtxin_pco(&v,4,nir_imm_int(&v,moves.position_input*4)),15);
 nir_store_var(&v,color,nir_load_vtxin_pco(&v,4,nir_imm_int(&v,moves.color_input*4)),15);
 nir_jump(&v,nir_jump_return);vd.common.vtxins=8;
 nir_frag_store_pco(&f,nir_pack_unorm_4x8(&f,nir_swizzle(&f,nir_load_var(&f,input),bgra,4)),.base=0);
 nir_jump(&f,nir_jump_return);
 components=glsl_get_vector_elements(color->type);
 pco_preprocess_nir(ctx,v.shader,&vd);pco_preprocess_nir(ctx,f.shader,&fd);
 pco_link_nir(ctx,v.shader,f.shader,&vd,&fd);pco_rev_link_nir(ctx,v.shader,f.shader);
 pco_lower_nir(ctx,v.shader,&vd);pco_postprocess_nir(ctx,v.shader,&vd);
 pco_lower_nir(ctx,f.shader,&fd);pco_postprocess_nir(ctx,f.shader,&fd);
 /* Allocate from the linked, live IO. Z/W coefficient use is compiler output,
  * never guessed from an offline fixture or a runtime shader name. */
 if(components!=4||!fd.fs.uses.w||fd.fs.uses.z){status=GEO_UNSUPPORTED;goto done;}
 vd.vs.varyings[VARYING_SLOT_POS]=(pco_range){.start=0,.count=4};
 vd.vs.varyings[VARYING_SLOT_VAR0]=(pco_range){.start=4,.count=components};
 vd.vs.vtxouts=4+components;vd.vs.f32_smooth=components;
 fd.fs.varyings[VARYING_SLOT_POS]=(pco_range){.start=0,.count=4u*(fd.fs.uses.z+fd.fs.uses.w)};
 fd.fs.varyings[VARYING_SLOT_VAR0]=(pco_range){.start=4u*(fd.fs.uses.z+fd.fs.uses.w),.count=4u*components};
 fd.common.coeffs=fd.fs.varyings[VARYING_SLOT_VAR0].start+fd.fs.varyings[VARYING_SLOT_VAR0].count;
 status=encode_pair_stage(ctx,v.shader,&vd,&made.vs);if(status)goto done;
 status=encode_pair_stage(ctx,f.shader,&fd,&made.ps);if(status)goto done;
 vs=pco_shader_data(made.vs.storage);fs=pco_shader_data(made.ps.storage);
 made.link.size=sizeof(made.link);made.link.version=2;made.link.varying_count=1;
 made.link.vs_vtxouts=vs->vs.vtxouts;made.link.vs_f32_smooth=vs->vs.f32_smooth;
 made.link.vs_f32_flat=vs->vs.f32_flat;made.link.vs_f32_npc=vs->vs.f32_npc;
 made.link.vs_f16_smooth=vs->vs.f16_smooth;made.link.vs_f16_flat=vs->vs.f16_flat;made.link.vs_f16_npc=vs->vs.f16_npc;
 made.link.position=snapshot_range(vs->vs.varyings[VARYING_SLOT_POS]);
 made.link.fs_coeffs=fs->common.coeffs;made.link.fs_uses_z=fs->fs.uses.z;made.link.fs_uses_w=fs->fs.uses.w;
 made.link.vs_entry_offset=vs->common.entry_offset;made.link.fs_entry_offset=fs->common.entry_offset;
 made.link.varying[0].semantic=GEO_DRAW_SEMANTIC_COLOR;made.link.varying[0].components=components;
 made.link.varying[0].bit_size=32;made.link.varying[0].interpolation=GEO_DRAW_INTERP_SMOOTH;
 made.link.varying[0].vs_output=snapshot_range(vs->vs.varyings[VARYING_SLOT_VAR0]);
 made.link.varying[0].fs_coeff=snapshot_range(fs->fs.varyings[VARYING_SLOT_VAR0]);
 *out=made;memset(&made,0,sizeof(made));status=GEO_OK;
 done:ralloc_free(v.shader);ralloc_free(f.shader);ralloc_free(made.vs.storage);ralloc_free(made.ps.storage);return status;
}
