/* SPDX-License-Identifier: MIT
 * Canonical re-encoding from Mesa PCO map/ISA source, not a shader whitelist.
 * No allocator, locks, user callbacks or device accesses in this validator. */
/* Isolated metadata ABI: never bind against an opaque cached PCO build's
 * debug-dependent pco_op_info layout. */
#ifndef NDEBUG
#define NDEBUG 1
#endif
#define pco_op_info geo_witness_op_info
#define pco_op_mod_info geo_witness_op_mod_info
#define pco_ref_mod_info geo_witness_ref_mod_info
/* freestanding equivalent of the CRT first-set-bit helper used by Mesa.
 * Source-only integer implementation; kernel has no user CRT import. */
int geo_witness_ffsll(long long);
#define ffsll geo_witness_ffsll
#include "../frontend/shader_witness.h"
#include "../clear_protocol.h"
#include "pco/pco_internal.h"
#include <string.h>
/* The reviewed mappings only need bounded local instruction scratch. Replace
 * their IR bookkeeping/arena allocation, not the instruction field encoders. */
struct local_scope {unsigned used;pco_instr instr[3];pco_ref dest[3][2],src[3][3];};
static pco_instr *local_create(pco_func *f,unsigned nd,unsigned ns) {
 struct local_scope *s=(void *)f;pco_instr *i;
 if(s->used>=3||nd>2||ns>3)return NULL;
 i=&s->instr[s->used];memset(i,0,sizeof(*i));i->dest=s->dest[s->used];i->src=s->src[s->used++];
 i->num_dests=nd;i->num_srcs=ns;i->parent_func=f;list_inithead(&i->link);return i;
}
#define pco_instr_create local_create
#define pco_instr_delete(i) ((void)(i))
#define ralloc_steal(context,ptr) ((void)(context),(void)(ptr))
#include "pco/pco_map.h"
#undef pco_instr_create
#undef pco_instr_delete
#undef ralloc_steal

int geo_witness_ffsll(long long bits) {
 unsigned long long value=(unsigned long long)bits;int index=1;
 if(!value)return 0;
 while(!(value&1u)){value>>=1;index++;}
 return index;
}
static unsigned count_bits(uint32_t m){unsigned n=0;while(m){n++;m>>=1;}return n;}
static int ref_read(pco_ref *to,const struct geo_shader_ref *s) {
 if(s->type>=_PCO_REF_TYPE_COUNT||s->bank>=_PCO_REG_CLASS_COUNT||s->channels>1023||s->bits>=_PCO_BITS_COUNT||s->dtype>=_PCO_DTYPE_COUNT||(s->modifiers&~511u))return 0;
 memset(to,0,sizeof(*to));to->val=s->value;to->type=s->type;to->reg_class=s->bank;to->chans=s->channels;to->bits=s->bits;to->dtype=s->dtype;
 to->oneminus=s->modifiers&1;to->clamp=(s->modifiers>>1)&1;to->flr=(s->modifiers>>2)&1;to->abs=(s->modifiers>>3)&1;to->neg=(s->modifiers>>4)&1;to->elem=(s->modifiers>>5)&15;
 return to->elem==0; /* No subword/element/half precision operations. */
}
static int reg_valid(pco_ref r,unsigned width,int dst,const struct geo_shader_metadata *m,uint32_t defined[4],uint32_t *constants) {
 unsigned n;
 if(r.type!=PCO_REF_TYPE_REG||r.bits!=PCO_BITS_32||pco_ref_get_chans(r)>4)return 0;
 if(width<pco_ref_get_chans(r))width=pco_ref_get_chans(r);
 if(width>4||r.val>256u-width)return 0;
 switch(r.reg_class){
 case PCO_REG_CLASS_TEMP:
  if(r.val+width>m->temps)return 0;
  for(n=r.val;n<r.val+width;n++){if(dst)defined[n/32]|=1u<<(n%32);else if(!(defined[n/32]&(1u<<(n%32))))return 0;}
  return 1;
 case PCO_REG_CLASS_VTXIN:return !dst&&!m->stage&&(m->input_mask&1u)&&r.val+width<=m->vtxins;
 case PCO_REG_CLASS_SHARED:
  if(dst||r.val+width>m->shareds)return 0;
  for(n=r.val;n<r.val+width;n++)*constants|=1u<<(n/4);
  return 1;
 case PCO_REG_CLASS_CONST:
  /* Mesa pco_const_imms.c: immutable numeric ROM, not arbitrary SPECIAL. */
  return !dst&&(r.val+width<=32u||(r.val>=64u&&r.val+width<=96u)||(r.val>=128u&&r.val+width<=160u));
 case PCO_REG_CLASS_PIXOUT:return m->stage&&r.val==0&&width==1&&(dst||m->framebuffer_fetch==1);
 default:return 0;
 }
}
static int modifiers_valid(pco_instr *i,const struct geo_shader_insn *w) {
 const struct pco_op_info *info=&pco_op_info[i->op];uint32_t used=0;
 for(unsigned mod=1;mod<_PCO_OP_MOD_COUNT;mod++)if(info->mods&(UINT64_C(1)<<mod)){
  unsigned idx=info->mod_map[mod];uint32_t v;
  if(!idx||idx>19)return 0;
  used|=1u<<(idx-1);v=w->modifiers[idx-1];
  switch(mod){
  case PCO_OP_MOD_RPT:if(v<1||v>4)return 0;break;
  case PCO_OP_MOD_EXEC_CND:if(v!=PCO_EXEC_CND_E1_ZX)return 0;break;
  case PCO_OP_MOD_PCK_FMT:if(v!=PCO_PCK_FMT_U8888)return 0;break;
  case PCO_OP_MOD_ATOM:case PCO_OP_MOD_LP:if(v)return 0;break;
  case PCO_OP_MOD_SAT:case PCO_OP_MOD_SCALE:case PCO_OP_MOD_ROUNDZERO:case PCO_OP_MOD_OLCHK:case PCO_OP_MOD_END:case PCO_OP_MOD_PHASE2END:if(v>1)return 0;break;
  default:if(v!=pco_op_mod_info[mod].nzdefault)return 0;break;
  }
 }
 for(unsigned n=0;n<19;n++){if(!(used&(1u<<n))&&w->modifiers[n])return 0;i->mod[n]=w->modifiers[n];}
 return 1;
}
static int make_group(pco_igrp *g,struct local_scope *scope,const struct geo_shader_insn *w,const struct geo_shader_metadata *m,uint32_t defined[4],uint32_t *constants,uint32_t *outputs,int last) {
 pco_instr i={0};pco_ref d[2],s[3];unsigned repeat=1;memset(d,0,sizeof(d));memset(s,0,sizeof(s));
 if(w->opcode>=_PCO_OP_COUNT||w->num_dests>2||w->num_srcs>3)return 0;
 switch(w->opcode){case PCO_OP_FADD:case PCO_OP_FMUL:case PCO_OP_FMAD:case PCO_OP_MBYP:case PCO_OP_PCK:case PCO_OP_UNPCK:case PCO_OP_MOVI32:case PCO_OP_MOVWM:case PCO_OP_MOVS1:case PCO_OP_UVSW_WRITE:case PCO_OP_UVSW_WRITE_EMIT_ENDTASK:case PCO_OP_UVSW_EMIT_ENDTASK:break;default:return 0;}
 const struct pco_op_info *info=&pco_op_info[w->opcode];
 if(info->num_dests!=w->num_dests||info->num_srcs!=w->num_srcs)return 0;
 i.op=w->opcode;i.num_dests=w->num_dests;i.num_srcs=w->num_srcs;i.dest=d;i.src=s;i.parent_func=(void *)scope;list_inithead(&i.link);
 if(!modifiers_valid(&i,w))return 0;
 if(info->mods&(UINT64_C(1)<<PCO_OP_MOD_RPT))repeat=pco_instr_get_mod(&i,PCO_OP_MOD_RPT);
 int end=(info->mods&(UINT64_C(1)<<PCO_OP_MOD_END))?pco_instr_get_mod(&i,PCO_OP_MOD_END):0;
 if(end!=last)return 0;
 if(!last&&(i.op==PCO_OP_UVSW_WRITE_EMIT_ENDTASK||i.op==PCO_OP_UVSW_EMIT_ENDTASK))return 0;
 for(unsigned n=0;n<2;n++){struct geo_shader_ref zero={0};if(n>=w->num_dests&&memcmp(&zero,&w->dest[n],sizeof(zero)))return 0;if(!ref_read(&d[n],&w->dest[n]))return 0;}
 for(unsigned n=0;n<3;n++){struct geo_shader_ref zero={0};if(n>=w->num_srcs&&memcmp(&zero,&w->src[n],sizeof(zero)))return 0;if(!ref_read(&s[n],&w->src[n]))return 0;}
 for(unsigned n=0;n<w->num_srcs;n++){
  if(s[n].type==PCO_REF_TYPE_IMM){
   if(s[n].neg||s[n].abs||s[n].flr||s[n].oneminus||s[n].clamp||s[n].chans||s[n].reg_class)return 0;
   if(i.op==PCO_OP_MOVI32&&n==0&&s[n].bits==PCO_BITS_32)continue;
   if(!m->stage&&n==1&&s[n].bits==PCO_BITS_8&&(i.op==PCO_OP_UVSW_WRITE||i.op==PCO_OP_UVSW_WRITE_EMIT_ENDTASK)&&s[n].val<4&&repeat<=4-s[n].val){*outputs|=((1u<<repeat)-1u)<<s[n].val;continue;}
   return 0;
  }
  if(!reg_valid(s[n],i.op==PCO_OP_UNPCK?1:repeat,0,m,defined,constants))return 0;
  if(s[n].type==PCO_REF_TYPE_REG&&s[n].reg_class==PCO_REG_CLASS_PIXOUT){
   if(i.op!=PCO_OP_MBYP||!(info->mods&(UINT64_C(1)<<PCO_OP_MOD_OLCHK))||!pco_instr_get_mod(&i,PCO_OP_MOD_OLCHK))return 0;
   *outputs|=16u;
  }
  enum pco_io map=i.op==PCO_OP_PCK?PCO_IO_S2:i.op==PCO_OP_MOVS1?PCO_IO_S1:(enum pco_io)(PCO_IO_S0+n);
  if(!ref_src_map_valid(s[n],map,NULL))return 0;
  uint64_t mask=info->src_mods[n];
  if((s[n].oneminus&&!(mask&(1u<<PCO_REF_MOD_ONEMINUS)))||(s[n].clamp&&!(mask&(1u<<PCO_REF_MOD_CLAMP)))||(s[n].flr&&!(mask&(1u<<PCO_REF_MOD_FLR)))||(s[n].abs&&!(mask&(1u<<PCO_REF_MOD_ABS)))||(s[n].neg&&!(mask&(1u<<PCO_REF_MOD_NEG))))return 0;
 }
 for(unsigned n=0;n<w->num_dests;n++){
  if(d[n].oneminus||d[n].clamp||d[n].flr||d[n].abs||d[n].neg||!reg_valid(d[n],repeat,1,m,defined,constants))return 0;
  if(d[n].reg_class==PCO_REG_CLASS_PIXOUT)*outputs|=15;
 }
 if((i.op==PCO_OP_UVSW_WRITE||i.op==PCO_OP_UVSW_WRITE_EMIT_ENDTASK||i.op==PCO_OP_UVSW_EMIT_ENDTASK)&&m->stage)return 0;
 if(last&&!m->stage&&i.op!=PCO_OP_UVSW_WRITE_EMIT_ENDTASK&&i.op!=PCO_OP_UVSW_EMIT_ENDTASK)return 0;
 memset(g,0,sizeof(*g));memset(scope,0,sizeof(*scope));
 switch(i.op){
 #define MAP(op,name) case PCO_OP_##op:pco_##name##_map_igrp(g,&i);break
 MAP(FADD,fadd);MAP(FMUL,fmul);MAP(FMAD,fmad);MAP(MBYP,mbyp);MAP(PCK,pck);MAP(UNPCK,unpck);MAP(MOVI32,movi32);MAP(MOVWM,movwm);MAP(MOVS1,movs1);MAP(UVSW_WRITE,uvsw_write);MAP(UVSW_WRITE_EMIT_ENDTASK,uvsw_write_emit_endtask);MAP(UVSW_EMIT_ENDTASK,uvsw_emit_endtask);
 #undef MAP
 default:return 0;
 }
 /* map functions reference i/d/s; keep their data in caller-owned scope. */
 pco_foreach_phase_in_igrp(g,phase)if(g->instrs[phase]==&i){if(scope->used>=3)return 0;pco_instr *copy=&scope->instr[scope->used];*copy=i;copy->dest=scope->dest[scope->used];copy->src=scope->src[scope->used];memcpy(copy->dest,d,sizeof(d));memcpy(copy->src,s,sizeof(s));g->instrs[phase]=copy;scope->used++;}
 /* Calculate exactly the source grouping lengths/DA, excluding final align. */
 g->enc.len.hdr=pco_igrp_hdr_bytes(g->variant.hdr);g->enc.len.lower_srcs=pco_src_bytes(g->variant.lower_src);g->enc.len.upper_srcs=pco_src_bytes(g->variant.upper_src);g->enc.len.iss=pco_iss_bytes(g->variant.iss);g->enc.len.dests=pco_dst_bytes(g->variant.dest);
 unsigned len=g->enc.len.hdr+g->enc.len.lower_srcs+g->enc.len.upper_srcs+g->enc.len.iss+g->enc.len.dests,da=g->enc.len.hdr;
 pco_foreach_phase_in_igrp(g,p){unsigned n=g->hdr.alutype==PCO_ALUTYPE_BITWISE?pco_bitwise_bytes(g->variant.instr[p].bitwise):p==PCO_OP_PHASE_BACKEND?pco_backend_bytes(g->variant.instr[p].backend):pco_main_bytes(g->variant.instr[p].main);g->enc.len.instrs[p]=n;len+=n;if(g->hdr.alutype==PCO_ALUTYPE_BITWISE||p>PCO_OP_PHASE_1)da+=n;}
 g->enc.len.word_padding=len%2;len+=len%2;g->enc.len.total=len;g->hdr.da=da;g->hdr.length=len/2;
 return len<=32&&da<=15;
}
static unsigned encode(uint8_t out[40],pco_igrp *g,unsigned align) {
 unsigned at=0;memset(out,0,40);g->hdr.length=(g->enc.len.total+align)/2;
 at+=pco_igrp_hdr_map_encode(out+at,g);
 pco_foreach_phase_in_igrp_rev(g,p)at+=pco_instr_map_encode(out+at,g,p);
 if(g->enc.len.lower_srcs)at+=pco_srcs_map_encode(out+at,g,0);
 if(g->enc.len.upper_srcs)at+=pco_srcs_map_encode(out+at,g,1);
 if(g->enc.len.iss)at+=pco_iss_map_encode(out+at,g);
 if(g->enc.len.dests)at+=pco_dests_map_encode(out+at,g);
 if(g->enc.len.word_padding)out[at++]=255;
 if(align){out[at++]=0xf0u|align/2;for(unsigned n=1;n<align;n++)out[at++]=255;}
 return at;
}
int32_t geo_shader_witness_measure(const void *data,uint32_t bytes,uint32_t *temps,uint32_t *vtxins) {
 const struct geo_shader_witness *w=data;const struct geo_shader_insn *rows;
 if(!w||((uintptr_t)w&3)||!temps||!vtxins||bytes<sizeof(*w)||w->count>128||bytes!=sizeof(*w)+w->count*sizeof(*rows))return GEO_INVALID;
 *temps=*vtxins=0;rows=(const void *)(w+1);
 for(unsigned n=0;n<w->count;n++){
  if(rows[n].opcode>=_PCO_OP_COUNT)return GEO_INVALID;
  const struct pco_op_info *info=&pco_op_info[rows[n].opcode];unsigned repeat=1;
  if(info->mods&(UINT64_C(1)<<PCO_OP_MOD_RPT)){unsigned index=info->mod_map[PCO_OP_MOD_RPT];if(!index||index>19)return GEO_INVALID;repeat=rows[n].modifiers[index-1];if(repeat<1||repeat>4)return GEO_INVALID;}
  for(unsigned j=0;j<5;j++){
   const struct geo_shader_ref *r=j<2?&rows[n].dest[j]:&rows[n].src[j-2];
   if(r->type!=PCO_REF_TYPE_REG)continue;
   unsigned width=r->channels+1;if(width<repeat&&!(j>=2&&rows[n].opcode==PCO_OP_UNPCK))width=repeat;
   if(width>4||r->value>256-width)return GEO_INVALID;
   unsigned end=r->value+width;
   if(r->bank==PCO_REG_CLASS_TEMP&&end>*temps)*temps=end;
   if(r->bank==PCO_REG_CLASS_VTXIN&&end>*vtxins)*vtxins=end;
  }
 }
 return GEO_OK;
}
int32_t geo_shader_witness_validate(const void *data,uint32_t bytes,const void *code,uint32_t codebytes,const struct geo_shader_metadata *m) {
 const struct geo_shader_witness *w=data;const struct geo_shader_insn *ins;uint32_t defined[4]={0},constants=0,outputs=0;unsigned total=0,lastlen=0,penpad=0,lastpad=0,at=0;
 if(!w||!code||!m||((uintptr_t)w&3)||bytes<sizeof(*w)||!codebytes||codebytes>GEO_SHADER_WITNESS_MAX_CODE||(codebytes&7)||m->stage>1||m->framebuffer_fetch>1||(!m->stage&&m->framebuffer_fetch)||m->temps>128||m->shareds>16||m->vtxins>(m->stage?0u:4u)||m->vtxouts!=(m->stage?0u:4u)||(m->input_mask&~(m->stage?0u:1u))||(m->constant_mask&~15u)||m->shareds!=4*count_bits(m->constant_mask))return GEO_INVALID;
 if(w->magic!=GEO_SHADER_WITNESS_MAGIC||w->version!=1||w->isa!=GEO_SHADER_WITNESS_ISA||w->stage!=m->stage||w->reserved||!w->count||w->count>128||bytes!=sizeof(*w)+w->count*sizeof(*ins))return GEO_INVALID;
 ins=(const void *)(w+1);
 for(unsigned n=0;n<w->count;n++){pco_igrp g;struct local_scope scope;if(!make_group(&g,&scope,ins+n,m,defined,&constants,&outputs,n+1==w->count))return GEO_UNSUPPORTED;total+=g.enc.len.total;lastlen=g.enc.len.total;}
 if((outputs&15u)!=15u||((outputs&16u)&&!m->framebuffer_fetch)||(constants&~m->constant_mask))return GEO_UNSUPPORTED;
 lastpad=(8-lastlen%8)%8;penpad=(8-(total-lastlen)%8)%8;
 if(w->count==1&&penpad)return GEO_INVALID;
 if(total+lastpad+penpad!=codebytes)return GEO_INVALID;
 memset(defined,0,sizeof(defined));constants=outputs=0;
 for(unsigned n=0;n<w->count;n++){
  pco_igrp g;struct local_scope scope;uint8_t encoded[40];unsigned pad=n+1==w->count?lastpad:n+2==w->count?penpad:0;
  if(!make_group(&g,&scope,ins+n,m,defined,&constants,&outputs,n+1==w->count)||g.enc.len.total+pad>32)return GEO_UNSUPPORTED;
  unsigned len=encode(encoded,&g,pad);if(len>codebytes-at)return GEO_INVALID;
  if(memcmp(encoded,(const uint8_t *)code+at,len))return GEO_INVALID;
  at+=len;
 }
 return at==codebytes?GEO_OK:GEO_INVALID;
}
