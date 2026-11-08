/* SPDX-License-Identifier: MIT
 * Same PCO source passes as pco_ir.c, with immutable pre-group IR export.
 * Existing compiler libraries are reused; no shader or USC substitution. */
#include "../frontend/shader_witness.h"
#include "../clear_protocol.h"
#include "pco/pco_internal.h"
#include "util/ralloc.h"
#include <string.h>
static void export_ref(struct geo_shader_ref *to,pco_ref r) {
 to->value=r.val;to->type=r.type;to->bank=r.reg_class;to->channels=r.chans;to->bits=r.bits;to->dtype=r.dtype;
 to->modifiers=r.oneminus|(r.clamp<<1)|(r.flr<<2)|(r.abs<<3)|(r.neg<<4)|((uint32_t)r.elem<<5);
}
int32_t geo_pco_process_witness_ir(pco_ctx *ctx,pco_shader *shader,uint32_t stage,void **output,uint32_t *bytes) {
 unsigned count=0,functions=0,blocks=0;bool progress;struct geo_shader_witness *w;struct geo_shader_insn *records;
 if(!output||!bytes)return GEO_INVALID;
 *output=NULL;*bytes=0;
 /* pco_ir.c pipeline, in the original order, through pco_end. */
 pco_validate_shader(shader,"before witness passes");
 PCO_PASS(_,shader,pco_const_imms);PCO_PASS(_,shader,pco_opt);
 do{progress=false;PCO_PASS(progress,shader,pco_dce);}while(progress);
 PCO_PASS(_,shader,pco_bool);PCO_PASS(_,shader,pco_cf);PCO_PASS(_,shader,pco_shrink_vecs);
 PCO_PASS(_,shader,pco_const_imms);PCO_PASS(_,shader,pco_opt_comp_only_vecs);PCO_PASS(_,shader,pco_opt);
 do{progress=false;PCO_PASS(progress,shader,pco_dce);}while(progress);
 PCO_PASS(_,shader,pco_schedule);PCO_PASS(_,shader,pco_pre_ra_legalize);PCO_PASS(_,shader,pco_ra);PCO_PASS(_,shader,pco_post_ra_legalize);PCO_PASS(_,shader,pco_end);
 pco_foreach_func_in_shader(f,shader){functions++;if(f->type!=PCO_FUNC_TYPE_ENTRYPOINT)return GEO_UNSUPPORTED;
  pco_foreach_block_in_func(b,f){blocks++;pco_foreach_instr_in_block(i,b){if(i->num_dests>2||i->num_srcs>3||++count>128)return GEO_UNSUPPORTED;}}
 }
 if(functions!=1||blocks!=1||!count)return GEO_UNSUPPORTED;
 *bytes=sizeof(*w)+count*sizeof(*records);w=rzalloc_size(shader,*bytes);if(!w)return GEO_BUSY;
 w->magic=GEO_SHADER_WITNESS_MAGIC;w->version=1;w->isa=GEO_SHADER_WITNESS_ISA;w->stage=stage;w->count=count;records=(void *)(w+1);count=0;
 pco_foreach_func_in_shader(f,shader)pco_foreach_instr_in_func(i,f){struct geo_shader_insn *r=records+count++;
  r->opcode=i->op;r->num_dests=i->num_dests;r->num_srcs=i->num_srcs;
  for(unsigned n=0;n<i->num_dests;n++)export_ref(&r->dest[n],i->dest[n]);
  for(unsigned n=0;n<i->num_srcs;n++)export_ref(&r->src[n],i->src[n]);
  memcpy(r->modifiers,i->mod,sizeof(r->modifiers));
 }
 PCO_PASS(_,shader,pco_group_instrs);pco_validate_shader(shader,"after witness grouping");
 *output=w;return GEO_OK;
}
