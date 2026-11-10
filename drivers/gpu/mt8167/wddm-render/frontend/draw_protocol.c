/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "draw_abi.h"
static int span(const struct geo_draw_span *s,uint32_t total,uint32_t maximum) {
 return s->bytes && !(s->offset&3u) && !(s->bytes&3u) && s->bytes<=maximum && s->offset<=total && s->bytes<=total-s->offset;
}
static uint32_t word(const unsigned char *b,uint32_t n) {
 return (uint32_t)b[n]|((uint32_t)b[n+1]<<8)|((uint32_t)b[n+2]<<16)|((uint32_t)b[n+3]<<24);
}
static int sm4_framing(const unsigned char *b,uint32_t off,uint32_t bytes) {
 uint32_t n=bytes/4u,i=2u;
 while(i<n) {
  uint32_t token=word(b,off+4u*i),len=(token>>24)&127u;
  /* CUSTOMDATA has its DWORD count in the next word. */
  if((token&0x7ffu)==53u) {
   if(i+1u>=n)return 0;
   len=word(b,off+4u*(i+1u));
   if(len<2u)return 0;
  }
  if(!len || len>n-i)return 0;
  i+=len;
 }
 return i==n;
}
static int shader(const struct geo_draw_shader *s,const unsigned char *b,uint32_t total,uint32_t stage) {
 uint32_t ver;
 if(s->stage!=stage || !span(&s->tokens,total,GEO_DRAW_MAX_TOKENS*4u) ||
    !span(&s->usc,total,GEO_DRAW_MAX_CODE) || (s->usc.bytes&7u) ||
    s->constants.bytes!=32u*16u || !span(&s->constants,total,32u*16u) ||
    s->temps>128u || s->shareds>128u || s->vtxins>64u || s->vtxouts>64u ||
    (s->input_mask&0xffff0000u))return 0;
 ver=word(b,s->tokens.offset);
 if(s->format==GEO_SHADER_D3D9_SM2) {
  if(ver!=(stage?0xffff0200u:0xfffe0200u) ||
     word(b,s->tokens.offset+s->tokens.bytes-4u)!=0xffffu)return 0;
 } else if(s->format==GEO_SHADER_D3D10_TOKENS) {
  uint32_t major=(ver>>4)&15u,minor=ver&15u;
  /* WDK tokenized program, not DXBC container and not SM2 re-tagged. */
  if(s->tokens.bytes<8u || (ver>>16)!=(stage?0u:1u) || (ver&0xff00u) ||
     !((major==4u && minor<=1u)||(major==5u && minor==0u)) ||
     word(b,s->tokens.offset+4u)!=s->tokens.bytes/4u ||
     !sm4_framing(b,s->tokens.offset,s->tokens.bytes))return 0;
 } else return 0;
 return 1;
}
int32_t geo_draw_validate_capture(const struct geo_draw_request *r,const void *blob,uint32_t bytes) {
 const unsigned char *b=blob;uint32_t i;
 if(!r || !b || r->size!=sizeof(*r) || r->version!=GEO_DRAW_ABI_VERSION || r->reserved ||
    r->blob_bytes!=bytes || !bytes || bytes>GEO_DRAW_MAX_BLOB || !r->target || !r->vertex ||
    r->target==r->vertex || !geo_desc_valid(&r->target_desc) || !geo_usage_renderable(r->target_desc.usage))return GEO_INVALID;
 /* V1 non-indexed float4 POSITION0 triangle list. No texture/blend/depth/clip
  * success claim; E may support a narrower profile and must reject it honestly. */
 if(r->state.topology!=GEO_DRAW_TRIANGLE_LIST || !r->state.vertex_count || r->state.vertex_count%3u ||
    r->state.vertex_stride!=16u || (r->state.vertex_offset&3u) ||
    r->state.cull || r->state.front_ccw || r->state.blend_enable ||
    r->state.color_write_mask!=15u || r->state.depth_enable || r->state.depth_write || r->state.stencil_enable)return GEO_UNSUPPORTED;
 {uint32_t clipped[4];if(geo_draw_scissor_bounds(&r->state,r->target_desc.width,r->target_desc.height,clipped)!=GEO_OK)return GEO_INVALID;}
 if(r->state.first_vertex>0xffffffffu-r->state.vertex_count ||
    r->state.first_vertex+r->state.vertex_count>(0xffffffffu-r->state.vertex_offset)/16u)return GEO_INVALID;
 for(i=0;i<6u;i++)if((r->state.viewport[i]&0x7f800000u)==0x7f800000u)return GEO_INVALID;
 if((r->state.viewport[2]&0x80000000u) || !(r->state.viewport[2]&0x7fffffffu) ||
    (r->state.viewport[3]&0x80000000u) || !(r->state.viewport[3]&0x7fffffffu))return GEO_INVALID;
 if((r->state.viewport[4]&0x80000000u)||(r->state.viewport[5]&0x80000000u)||
    r->state.viewport[4]>r->state.viewport[5]||r->state.viewport[5]>0x3f800000u)return GEO_INVALID;
 if(!shader(&r->vs,b,bytes,0u)||!shader(&r->ps,b,bytes,1u))return GEO_INVALID;
 return GEO_OK;
}
