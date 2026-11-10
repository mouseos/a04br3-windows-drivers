/* SPDX-License-Identifier: MIT */
#include "shader_draw_abi.h"
int32_t geo_draw_validate_capture_v4(const struct geo_draw_request_v4 *r,const void *blob,uint32_t bytes) {
 struct geo_draw_request base;const struct geo_draw_shader *stages[2];const unsigned char *b=blob;
 if(!r||!b||((uintptr_t)b&3)||r->base.size!=sizeof(*r)||r->base.version!=4||r->isa!=GEO_SHADER_WITNESS_ISA||r->reserved)return GEO_INVALID;
 base=r->base;base.size=sizeof(base);base.version=1;
 int32_t status=geo_draw_validate_capture(&base,blob,bytes);if(status)return status;
 stages[0]=&r->base.vs;stages[1]=&r->base.ps;
 for(unsigned n=0;n<2;n++){
  const struct geo_draw_span *p=&r->witness[n];const struct geo_draw_shader *s=stages[n];
  if(!p->bytes||(p->offset&3)||p->offset>bytes||p->bytes>bytes-p->offset||s->format!=GEO_SHADER_D3D10_TOKENS)return GEO_INVALID;
  struct geo_shader_metadata m={n,s->temps,s->shareds,s->vtxins,s->vtxouts,s->input_mask,s->constant_mask,n?r->base.state.blend_enable:0};
  status=geo_shader_witness_validate(b+p->offset,p->bytes,b+s->usc.offset,s->usc.bytes,&m);if(status)return status;
 }
 return GEO_OK;
}

int32_t geo_draw_validate_capture_v5(const struct geo_draw_request_v5 *r,const void *blob,uint32_t bytes) {
 struct geo_draw_request_v3 sampled;const unsigned char *b=blob;const struct geo_draw_shader *s;const struct geo_draw_span *p;
 if(!r||!b||((uintptr_t)b&3)||r->sampled.base.size!=sizeof(*r)||r->sampled.base.version!=5||r->isa!=GEO_SHADER_WITNESS_ISA||r->reserved)return GEO_INVALID;
 sampled=r->sampled;sampled.base.size=sizeof(sampled);sampled.base.version=3;
 int32_t status=geo_draw_validate_capture_v3(&sampled,blob,bytes);if(status)return status;
 s=&r->sampled.base.vs;p=&r->vertex_witness;
 if(!p->bytes||(p->offset&3)||p->offset>bytes||p->bytes>bytes-p->offset||s->format!=GEO_SHADER_D3D10_TOKENS)return GEO_INVALID;
 struct geo_shader_metadata m={0,s->temps,s->shareds,s->vtxins,s->vtxouts,s->input_mask,s->constant_mask,0};
 return geo_shader_witness_validate(b+p->offset,p->bytes,b+s->usc.offset,s->usc.bytes,&m);
}
