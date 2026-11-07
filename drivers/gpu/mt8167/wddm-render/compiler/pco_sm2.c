/* SPDX-License-Identifier: MIT */
/* Real compiler adapter. Build with the same reviewed patched Mesa PCO/NIR
 * source as the native shader producer, never with a generated USC lookup. */
#include "shader_compiler.h"
#include "../clear_protocol.h"
#include "pco/pco.h"
#include "pco/pco_data.h"
#include "pco/pco_internal.h"
#include "util/ralloc.h"
#include <string.h>
static nir_def *source(nir_builder *b,const struct geo_sm2_operand *s,nir_def **temp,nir_def **constants) {
 unsigned swizzle[4],i;nir_def *value;
 if(s->file==0)value=temp[s->index];
 else if(s->file==1)value=nir_load_vtxin_pco(b,4,nir_imm_int(b,s->index*4));
 else value=constants[s->index];
 for(i=0;i<4;i++)swizzle[i]=(s->swizzle>>(2*i))&3;
 value=nir_swizzle(b,value,swizzle,4);return s->negate?nir_fneg(b,value):value;
}
static int32_t compile_sm2(void *context,uint32_t stage,const uint32_t *tokens,uint32_t bytes,struct geo_compiled_shader *out,struct geo_sm2_error *error) {
 pco_ctx *ctx=context;pco_shader *shader;struct geo_sm2_program *program;nir_builder b;
 nir_def *temp[32]={0},*constants[32]={0},*output,*value,*s[3]={0};
 pco_data data={0};unsigned i,j,shareds=0,vtxins=0;int32_t status;
 if(!ctx||!out)return GEO_INVALID;
 memset(out,0,sizeof(*out));program=rzalloc(ctx,struct geo_sm2_program);if(!program)return GEO_BUSY;
 status=geo_sm2_decode(program,stage,tokens,bytes,error);if(status!=GEO_OK){ralloc_free(program);return status;}
 b=nir_builder_init_simple_shader(stage==GEO_SM2_VERTEX?MESA_SHADER_VERTEX:MESA_SHADER_FRAGMENT,pco_nir_options(),"D3D9-SM2-source");
 output=nir_undef(&b,4,32);
 for(i=0;i<32;i++)if(program->constant_mask&(1u<<i)) {
  constants[i]=nir_load_preamble(&b,4,32,.base=i*4);shareds=(i+1)*4;
 }
 for(i=0;i<16;i++)if(program->input_mask&(1u<<i))vtxins=(i+1)*4;
 for(i=0;i<program->count;i++) {
  const struct geo_sm2_instruction *p=&program->instructions[i];nir_def **destination;
  if(p->opcode==81) {
   constants[p->dst.index]=nir_vec4(&b,nir_imm_int(&b,p->immediate[0]),nir_imm_int(&b,p->immediate[1]),
     nir_imm_int(&b,p->immediate[2]),nir_imm_int(&b,p->immediate[3]));continue;
  }
  s[0]=source(&b,&p->src[0],temp,constants);
  if(p->opcode!=1)s[1]=source(&b,&p->src[1],temp,constants);
  if(p->opcode==4)s[2]=source(&b,&p->src[2],temp,constants);
  switch(p->opcode) {
   case 1:value=s[0];break;
   case 2:value=nir_fadd(&b,s[0],s[1]);break;
   case 4:value=nir_fadd(&b,nir_fmul(&b,s[0],s[1]),s[2]);break;
   case 5:value=nir_fmul(&b,s[0],s[1]);break;
   case 8:value=nir_replicate(&b,nir_fdot3(&b,s[0],s[1]),4);break;
   case 9:value=nir_replicate(&b,nir_fdot4(&b,s[0],s[1]),4);break;
   default:ralloc_free(b.shader);ralloc_free(program);return GEO_UNSUPPORTED;
  }
  if(p->dst.saturate)value=nir_fsat(&b,value);
  destination=p->dst.file==0?&temp[p->dst.index]:&output;
  if(!*destination)*destination=nir_undef(&b,4,32);
  for(j=0;j<4;j++)if(p->dst.mask&(1u<<j))*destination=nir_vector_insert_imm(&b,*destination,nir_channel(&b,value,j),j);
 }
 if(stage==GEO_SM2_VERTEX)nir_uvsw_write_pco(&b,nir_imm_int(&b,0),output);
 else {
  /* Target contract is BGRA8, unlike E's private RGBA fixture. Keep this
   * explicit in metadata; owner PBE/export setup must use the same layout. */
  unsigned bgra[4]={2,1,0,3};
  nir_frag_store_pco(&b,nir_pack_unorm_4x8(&b,nir_swizzle(&b,output,bgra,4)),.base=0);
 }
 nir_jump(&b,nir_jump_return);data.common.shareds=shareds;data.common.vtxins=vtxins;
 if(stage==GEO_SM2_VERTEX)data.vs.vtxouts=4;
 pco_preprocess_nir(ctx,b.shader,&data);pco_lower_nir(ctx,b.shader,&data);pco_postprocess_nir(ctx,b.shader,&data);
 shader=pco_trans_nir(ctx,b.shader,&data,NULL);
 if(!shader){ralloc_free(b.shader);ralloc_free(program);return GEO_UNSUPPORTED;}
 pco_process_ir(ctx,shader);pco_encode_ir(ctx,shader);
 out->size=sizeof(*out);out->version=1;out->stage=stage;out->code_bytes=pco_shader_binary_size(shader);
 out->code=pco_shader_binary_data(shader);out->storage=shader;
 out->temps=pco_shader_data(shader)->common.temps;out->shareds=pco_shader_data(shader)->common.shareds;
 out->vtxins=pco_shader_data(shader)->common.vtxins;out->vtxouts=stage==GEO_SM2_VERTEX?pco_shader_data(shader)->vs.vtxouts:0;
 memcpy(out->input_semantics,program->input_semantics,sizeof(out->input_semantics));
 out->input_mask=program->input_mask;out->constant_mask=program->constant_mask;out->output_format=stage==GEO_SM2_PIXEL?GEO_FORMAT_A8R8G8B8:0;
 ralloc_free(b.shader);ralloc_free(program);
 if(!out->code||!out->code_bytes){ralloc_free(shader);memset(out,0,sizeof(*out));return GEO_UNSUPPORTED;}
 return GEO_OK;
}
static void release_sm2(void *context,struct geo_compiled_shader *p) {
 (void)context;if(p){ralloc_free(p->storage);memset(p,0,sizeof(*p));}
}
int32_t geo_pco_compiler_init(struct geo_shader_compiler *out,void *context) {
 if(!out||!context)return GEO_INVALID;
 memset(out,0,sizeof(*out));out->size=sizeof(*out);out->version=1;out->context=context;
 out->compile=compile_sm2;out->release=release_sm2;return GEO_OK;
}
