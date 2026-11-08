/* SPDX-License-Identifier: MIT. WDK8.1 d3d10TokenizedProgramFormat.hpp. */
#include "texture2d_sm4.h"
#include "../clear_protocol.h"
int32_t geo_sm4_texture2d_decode(const uint32_t *t,uint32_t bytes,const struct geo_sm4_signatures *s,struct geo_texture2d_program *out)
{
 uint32_t at=2,flags=0,cb=0,sampled=0;struct geo_texture2d_program made={0};
 if(!t||!s||!out||((uintptr_t)t&3u)||(bytes&3u)||bytes<12||bytes/4>GEO_SM2_MAX_TOKENS)return GEO_INVALID;
 if(t[0]!=0x40u||t[1]!=bytes/4)return GEO_UNSUPPORTED;
 if(s->input_count||s->output_count!=1||s->outputs[0].reg||s->outputs[0].mask!=15||s->outputs[0].system_value)return GEO_UNSUPPORTED;
 while(at<bytes/4){
  uint32_t begin=at,v=t[at++],op=v&2047u,len=(v>>24)&127u,end;
  if(!len||len>bytes/4-begin)return GEO_INVALID;end=begin+len;
  if(v&0x80000000u)return GEO_UNSUPPORTED;
  if(op==62u){if(v!=0x0100003eu||end!=bytes/4||!sampled||flags!=15)return GEO_INVALID;*out=made;return GEO_OK;}
  if(sampled)return GEO_UNSUPPORTED;
  if(op==106u){if(v!=0x0100086au)return GEO_UNSUPPORTED;continue;} /* refactoringAllowed */
  if(op==88u){ /* resource DIMENSION_TEXTURE2D3 / FLOAT5555, t0 */
   if((flags&1)||v!=0x04001858u||t[at]!=0x00107000u||t[at+1]||t[at+2]!=0x5555u)return GEO_UNSUPPORTED;
   flags|=1;at=end;continue;
  }
  if(op==90u){ /* default (not comparison) sampler s0, zero-component operand */
   if((flags&2)||v!=0x0300005au||t[at]!=0x00106000u||t[at+1])return GEO_UNSUPPORTED;
   flags|=2;at=end;continue;
  }
  if(op==89u){
   if((flags&4)||v!=0x04000059u||t[at]!=0x00208e46u||t[at+1]||!(cb=t[at+2])||cb>30)return GEO_UNSUPPORTED;
   flags|=4;at=end;continue;
  }
  if(op==101u){if((flags&8)||v!=0x03000065u||t[at]!=0x001020f2u||t[at+1])return GEO_UNSUPPORTED;flags|=8;at=end;continue;}
  if(op==69u){
   uint32_t coord,swizzle;
   if(flags!=15||v!=0x0a000045u||t[at++]!=0x001020f2u||t[at++])return GEO_UNSUPPORTED;
   coord=t[at++];
   /* CB0[immediate], float vector swizzle only. No relative index/modifier. */
   if((coord&~0x00000ff0u)!=0x00208006u||t[at++])return GEO_UNSUPPORTED;
   made.coordinate_constant=t[at++];if(made.coordinate_constant>=cb)return GEO_INVALID;
   swizzle=(coord>>4)&255u;made.coordinate_x=swizzle&3u;made.coordinate_y=(swizzle>>2)&3u;
   if(t[at++]!=0x00107e46u||t[at++]||t[at++]!=0x00106000u||t[at++])return GEO_UNSUPPORTED;
   if(at!=end)return GEO_INVALID;sampled=1;continue;
  }
  return GEO_UNSUPPORTED;
 }
 return GEO_INVALID;
}
