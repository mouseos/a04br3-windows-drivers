/* SPDX-License-Identifier: MIT. Diagnostic only; no owner/wire layout changes. */
#ifndef GEO_DRAW_TRACE_CANDIDATE_H
#define GEO_DRAW_TRACE_CANDIDATE_H
#include "../wddm-render/frontend/shader_draw_abi.h"
/* FNV-1a32 is a comparison checksum, never authentication or a unique ID.
 * All fixed records hash LE u32-only structs; owner IDs/VA/pointers excluded. */
static uint32_t GeoDrawTraceHash(uint32_t h,const void *p,uint32_t n) {
 const uint8_t *b=p;for(uint32_t i=0;i<n;i++)h=(h^b[i])*16777619u;return h;
}
static uint32_t GeoDrawTraceSpan(const struct geo_draw_span *s,const void *blob,uint32_t bytes) {
 if(s->offset>bytes||s->bytes>bytes-s->offset)return 0;
 return GeoDrawTraceHash(2166136261u,(const uint8_t *)blob+s->offset,s->bytes);
}
static const struct geo_draw_request_v3 *GeoDrawTraceSample(const struct geo_draw_request *r) {
 return r->version==5?&((const struct geo_draw_request_v5 *)r)->sampled:
        r->version==3?(const struct geo_draw_request_v3 *)r:0;
}
static uint32_t GeoDrawTraceMetadata(const struct geo_draw_request *r) {
 uint32_t h=GeoDrawTraceHash(2166136261u,&r->state,sizeof(r->state));
 h=GeoDrawTraceHash(h,&r->vs,sizeof(r->vs));return GeoDrawTraceHash(h,&r->ps,sizeof(r->ps));
}
static uint32_t GeoDrawTraceCookie(const struct geo_draw_request *r,const void *blob,uint32_t bytes) {
 uint32_t h=GeoDrawTraceHash(2166136261u,&r->version,4);
 h=GeoDrawTraceHash(h,&r->target_desc,sizeof(r->target_desc));
 uint32_t m=GeoDrawTraceMetadata(r);h=GeoDrawTraceHash(h,&m,4);
 const struct geo_draw_request_v3 *s=GeoDrawTraceSample(r);
 if(s){h=GeoDrawTraceHash(h,&s->texture_desc,sizeof(s->texture_desc));h=GeoDrawTraceHash(h,&s->sampler_profile,4);}
 h=GeoDrawTraceHash(h,blob,bytes);return h;
}
/* Must only be called for the retained validated immutable Draw capture at
 * PASSIVE. R69 commits metadata after reset records/anchor/payload writes. */
static void GeoDrawTraceSnapshot(const struct geo_draw_request *r,const void *blob,uint32_t bytes,
 uint32_t slot,uint32_t generation,uint32_t fence) {
 uint32_t cookie=GeoDrawTraceCookie(r,blob,bytes);const struct geo_draw_request_v3 *s=GeoDrawTraceSample(r);
 struct geo_draw_span witness={0};if(r->version==5)witness=((const struct geo_draw_request_v5 *)r)->vertex_witness;
 else if(r->version==4)witness=((const struct geo_draw_request_v4 *)r)->witness[0];
 for(uint32_t phase=52;phase<=54;phase++)GeoFactoryCheckpoint(phase,STATUS_PENDING,0,r->version,0,cookie);
 GeoFactoryCheckpoint(55,STATUS_PENDING,slot,generation,0,0);
 GeoFactoryCheckpoint(56,STATUS_SUCCESS,slot,generation,fence,cookie);
 GeoFactoryCheckpoint(57,STATUS_SUCCESS,r->version,bytes,GeoDrawTraceHash(2166136261u,blob,bytes),cookie);
 GeoFactoryCheckpoint(58,STATUS_SUCCESS,r->target_desc.width,r->target_desc.height,r->target_desc.pitch,cookie);
 GeoFactoryCheckpoint(63,STATUS_SUCCESS,s?s->texture_desc.width:0,s?s->texture_desc.height:0,s?s->texture_desc.pitch:0,cookie);
 GeoFactoryCheckpoint(64,STATUS_SUCCESS,r->vs.tokens.bytes,r->vs.usc.bytes,r->vs.constants.bytes,cookie);
 GeoFactoryCheckpoint(65,STATUS_SUCCESS,r->ps.tokens.bytes,r->ps.usc.bytes,r->ps.constants.bytes,cookie);
 GeoFactoryCheckpoint(66,STATUS_SUCCESS,GeoDrawTraceSpan(&r->vs.tokens,blob,bytes),GeoDrawTraceSpan(&r->vs.usc,blob,bytes),GeoDrawTraceSpan(&r->vs.constants,blob,bytes),cookie);
 GeoFactoryCheckpoint(67,STATUS_SUCCESS,GeoDrawTraceSpan(&r->ps.tokens,blob,bytes),GeoDrawTraceSpan(&r->ps.usc,blob,bytes),GeoDrawTraceSpan(&r->ps.constants,blob,bytes),cookie);
 GeoFactoryCheckpoint(68,STATUS_SUCCESS,witness.bytes,witness.bytes?GeoDrawTraceSpan(&witness,blob,bytes):0,s?s->sampler_profile:0,cookie);
 GeoFactoryCheckpoint(69,STATUS_SUCCESS,GeoDrawTraceHash(2166136261u,&r->target_desc,sizeof(r->target_desc)),s?GeoDrawTraceHash(2166136261u,&s->texture_desc,sizeof(s->texture_desc)):0,GeoDrawTraceMetadata(r),cookie);
}
#endif
