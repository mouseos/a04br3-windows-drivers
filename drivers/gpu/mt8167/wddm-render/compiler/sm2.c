/* SPDX-License-Identifier: MIT */
#include "sm2.h"
#include "../clear_protocol.h"
#include "../memory.h"
static uint32_t reg_file(uint32_t t){return ((t>>28)&7)|((t>>8)&24);}
static int operand(struct geo_sm2_operand *o,uint32_t t,int dst) {
 uint32_t allowed=0xf0001fffu|(dst?0x001f0000u:0x0fff0000u);
 if(!(t&0x80000000u)||(t&~allowed))return 0;
 memset(o,0,sizeof(*o));o->file=reg_file(t);o->index=t&2047;
 if(dst){o->mask=(t>>16)&15;o->saturate=(t>>20)&1;return o->mask!=0;}
 o->swizzle=(t>>16)&255;o->negate=(t>>24)&15;return o->negate<=1;
}
int32_t geo_sm2_decode(struct geo_sm2_program *p,uint32_t stage,const uint32_t *tokens,uint32_t bytes,struct geo_sm2_error *error) {
 uint32_t at=1,i,k,op,n,srcs,defined[32]={0},outputs=0,declared=0,used=0;struct geo_sm2_instruction *ins;
 if(error)memset(error,0,sizeof(*error));
 if(!p||!tokens||((uintptr_t)tokens&3)||stage>1||bytes<8||(bytes&3)||bytes/4>GEO_SM2_MAX_TOKENS)return GEO_INVALID;
 if(tokens[0]!=(stage==GEO_SM2_VERTEX?0xfffe0200u:0xffff0200u))return GEO_UNSUPPORTED;
 memset(p,0,sizeof(*p));p->stage=stage;
 while(at<bytes/4) {
  uint32_t start=at,t=tokens[at++];op=t&65535;
  if(error){error->word=start;error->opcode=op;}
  if(op==65535)return t==65535&&at==bytes/4&&outputs==15?GEO_OK:GEO_INVALID;
  if(op==65534) {n=(t>>16)&32767;if((t&0x80000000u)||n>bytes/4-at)return GEO_INVALID;at+=n;continue;}
  n=(t>>24)&15;
  if(t&0xf0ff0000u)return GEO_UNSUPPORTED; /* no coissue, predicates or controls */
  if(n>bytes/4-at)return GEO_INVALID;
  if(p->count==GEO_SM2_MAX_INSNS)return GEO_UNSUPPORTED;
  ins=&p->instructions[p->count];ins->opcode=op;
  if(op==31) { /* DCL: preserve semantic binding; no samplers/varying backend yet. */
   uint32_t semantic;
   if(stage!=GEO_SM2_VERTEX||n!=2||used)return GEO_UNSUPPORTED;
   semantic=tokens[at];
   if((semantic&15)>13||(semantic&~0x800f000fu)||!(semantic&0x80000000u)||!operand(&ins->dst,tokens[at+1],1)||
      ins->dst.file!=1||ins->dst.index>=16||ins->dst.mask!=15||ins->dst.saturate||declared&(1u<<ins->dst.index))return GEO_INVALID;
   p->input_semantics[ins->dst.index]=semantic;declared|=1u<<ins->dst.index;at+=n;continue;
  }
  if(op==81) { /* DEF is compile-time IEEE float bits, not a runtime pointer. */
   if(n!=5||used||!operand(&ins->dst,tokens[at],1)||ins->dst.file!=2||ins->dst.index>=GEO_SM2_CONSTANTS||
      ins->dst.mask!=15||ins->dst.saturate||p->immediate_mask&(1u<<ins->dst.index))return GEO_INVALID;
   p->immediate_mask|=1u<<ins->dst.index;
   for(i=0;i<4;i++)ins->immediate[i]=tokens[at+1+i];
   at+=n;p->count++;continue;
  }
  switch(op){case 1:srcs=1;break;case 2:case 5:case 8:case 9:srcs=2;break;case 4:srcs=3;break;default:return GEO_UNSUPPORTED;}
  if(n!=1+srcs||!operand(&ins->dst,tokens[at++],1))return GEO_INVALID;
  if(!((ins->dst.file==0&&ins->dst.index<32)||
       (ins->dst.file==(stage==GEO_SM2_VERTEX?4u:8u)&&!ins->dst.index)))return GEO_UNSUPPORTED;
  for(i=0;i<srcs;i++) {
   struct geo_sm2_operand *s=&ins->src[i];uint32_t reads=0;
   if(!operand(s,tokens[at++],0))return GEO_UNSUPPORTED;
   for(k=0;k<4;k++)reads|=1u<<((s->swizzle>>(2*k))&3);
   if(s->file==0){if(s->index>=32||(defined[s->index]&reads)!=reads)return GEO_INVALID;}
   else if(s->file==1){if(stage!=GEO_SM2_VERTEX||s->index>=16||!(declared&(1u<<s->index)))return GEO_UNSUPPORTED;p->input_mask|=1u<<s->index;}
   else if(s->file==2){if(s->index>=GEO_SM2_CONSTANTS)return GEO_UNSUPPORTED;if(!(p->immediate_mask&(1u<<s->index)))p->constant_mask|=1u<<s->index;}
   else return GEO_UNSUPPORTED;
  }
  if(ins->dst.file==0)defined[ins->dst.index]|=ins->dst.mask;else outputs|=ins->dst.mask;
  p->count++;used=1;
 }
 return GEO_INVALID; /* missing END */
}
