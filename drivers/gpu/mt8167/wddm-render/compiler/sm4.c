/* SPDX-License-Identifier: MIT */
#include "sm4.h"
#include "../clear_protocol.h"
#include "../memory.h"
/* WDK8.1 d3d10TokenizedProgramFormat.hpp opcode/operand bit fields. */
static int operand(struct geo_sm2_operand *o,const uint32_t *t,uint32_t *at,uint32_t end,int dst) {
 uint32_t v,type,dim,mode,index;
 if(*at>=end)return 0;
 v=t[(*at)++];type=(v>>12)&255u;dim=(v>>20)&3u;mode=(v>>2)&3u;
 /* Immediate indices only; no extended/relative/dynamic resource operands. */
 if((v&0xffc00000u)||(v&3u)!=2u||*at>=end)return 0;
 memset(o,0,sizeof(*o));index=t[(*at)++];
 if(type==8u) {
  if(dst||dim!=2u||index||*at>=end)return 0;
  o->file=2u;o->index=t[(*at)++];if(o->index>32u)return 0;
 } else {
  if(dim!=1u||type>2u)return 0;
  o->file=type==2u?8u:type;o->index=index;
 }
 if(dst){if(mode||!(o->mask=(v>>4)&15u)||(v&0xf00u))return 0;}
 else if(mode==1u)o->swizzle=(v>>4)&255u;
 else if(mode==2u){index=(v>>4)&3u;if(v&0xfc0u)return 0;o->swizzle=index*0x55u;}
 else return 0;
 return 1;
}
int32_t geo_sm4_decode(struct geo_sm2_program *p,uint32_t stage,const uint32_t *t,uint32_t bytes,
 const struct geo_sm4_signatures *sig,struct geo_sm2_error *error) {
 uint32_t at=2u,words=bytes/4u,i,k,used=0u,outputs=0u,declared=0u,cb_count=0u,temps=0u,out_declared=0u;
 uint32_t defined[32]={0};
 if(error)memset(error,0,sizeof(*error));
 if(!p||!t||!sig||((uintptr_t)t&3u)||stage>1u||bytes<12u||(bytes&3u)||words>GEO_SM2_MAX_TOKENS)return GEO_INVALID;
 /* Implement SM4.0 only; a framed 4.1/5.0 program is not compile support. */
 if(t[0]!=((stage?0u:1u)<<16|0x40u))return GEO_UNSUPPORTED;
 if(t[1]!=words||sig->input_count>16u||sig->output_count!=1u||
    sig->outputs[0].reg||sig->outputs[0].mask!=15u||sig->outputs[0].system_value!=(stage?0u:1u))return GEO_INVALID;
 if(stage?sig->input_count!=0u:(sig->input_count!=1u||sig->inputs[0].reg||sig->inputs[0].mask!=15u||sig->inputs[0].system_value))return GEO_UNSUPPORTED;
 memset(p,0,sizeof(*p));p->stage=stage;
 while(at<words) {
  uint32_t start=at,v=t[at++],op=v&0x7ffu,len=(v>>24)&127u,end,srcs=0u,lower=0u;
  struct geo_sm2_instruction *ins;
  if(error){error->word=start;error->opcode=op;}
  if(!len||len>words-start)return GEO_INVALID;
  end=start+len;if(v&0x80000000u)return GEO_UNSUPPORTED;
  if(op==62u)return len==1u&&!(v&0x00fff800u)&&end==words&&outputs==15u?GEO_OK:GEO_INVALID;
  if(op==106u) { /* dcl_globalFlags: only refactoringAllowed */
   if(used||len!=1u||(v&0x00fff000u))return GEO_UNSUPPORTED;
   continue;
  }
  if(op==104u) { /* dcl_temps */
   if(used||len!=2u||(v&0x00fff800u)||temps||!t[at]||t[at]>32u)return GEO_UNSUPPORTED;
   temps=t[at++];continue;
  }
  if(op==89u) { /* dcl_constantbuffer cb0[count], immediateIndexed */
   struct geo_sm2_operand cb;
   if(used||(v&0x00fff800u)||cb_count||!operand(&cb,t,&at,end,0)||cb.file!=2u||!cb.index||at!=end)return GEO_UNSUPPORTED;
   cb_count=cb.index;continue;
  }
  if(op==95u||op==101u||op==103u) { /* dcl_input/output/output_siv */
   struct geo_sm2_operand reg;
   if(used||(v&0x00fff800u)||!operand(&reg,t,&at,end,1)||reg.index||reg.mask!=15u)return GEO_UNSUPPORTED;
   if(op==95u){if(stage||reg.file!=1u||declared)return GEO_UNSUPPORTED;declared=1u;p->input_semantics[0]=0x80000000u;}
   else {
    if(reg.file!=8u||out_declared)return GEO_UNSUPPORTED;
    if(op==103u){if(stage||at>=end||t[at++]!=1u)return GEO_UNSUPPORTED;}
    else if(!stage)return GEO_UNSUPPORTED;
    out_declared=1u;
   }
   if(at!=end)return GEO_INVALID;
   continue;
  }
  switch(op){case 54u:srcs=1u;lower=1u;break;case 0u:srcs=2u;lower=2u;break;
   case 56u:srcs=2u;lower=5u;break;case 50u:srcs=3u;lower=4u;break;
   case 16u:srcs=2u;lower=8u;break;case 17u:srcs=2u;lower=9u;break;default:return GEO_UNSUPPORTED;}
  if((v&0x00ffd800u)||p->count==GEO_SM2_MAX_INSNS||!out_declared)return GEO_UNSUPPORTED;
  ins=&p->instructions[p->count];ins->opcode=lower;
  if(!operand(&ins->dst,t,&at,end,1))return GEO_UNSUPPORTED;
  ins->dst.saturate=(v>>13)&1u;
  if(ins->dst.file==0u){if(ins->dst.index>=temps)return GEO_INVALID;}
  else if(ins->dst.file==8u){if(ins->dst.index)return GEO_UNSUPPORTED;ins->dst.file=stage?8u:4u;}
  else return GEO_UNSUPPORTED;
  for(i=0;i<srcs;i++) {
   struct geo_sm2_operand *s=&ins->src[i];uint32_t reads=0u,lanes=ins->dst.mask;
   if(!operand(s,t,&at,end,0))return GEO_UNSUPPORTED;
   /* Component-wise float ops consume only written destination lanes.
    * Dot products consume XYZ/XYZW independently of the output mask. The
    * NIR lowerer inserts only dst.mask lanes, so unused swizzle lanes do not
    * make an otherwise valid partial temporary read undefined. */
   if(lower==8u)lanes=7u;else if(lower==9u)lanes=15u;
   for(k=0;k<4u;k++)if(lanes&(1u<<k))reads|=1u<<((s->swizzle>>(2u*k))&3u);
   if(s->file==0u){if(s->index>=temps||(defined[s->index]&reads)!=reads)return GEO_INVALID;}
   else if(s->file==1u){if(stage||s->index||!declared)return GEO_UNSUPPORTED;p->input_mask|=1u;}
   else if(s->file==2u){if(s->index>=cb_count)return GEO_INVALID;p->constant_mask|=1u<<s->index;}
   else return GEO_UNSUPPORTED;
  }
  if(at!=end)return GEO_INVALID;
  if(ins->dst.file==0u)defined[ins->dst.index]|=ins->dst.mask;else outputs|=ins->dst.mask;
  p->count++;used=1u;
 }
 return GEO_INVALID;
}
