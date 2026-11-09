/* SPDX-License-Identifier: MIT. Finite source-compiled sample admission only. */
#include "native_texture2d_program.h"
#include "../../wddm-render/compiler/texture2d_sm4.h"
/* Validation witness, not executable substitution. Source codegen SHA256:
 * e8f10f2555a7f3e02f47ee2fc9b74772c0a399204730662c19c6d1b2dc63d7ab */
static const uint8_t expected_sample_code[88]={
 0x35,0x82,0x00,0x87,0x88,0x08,0x00,0x00,0x00,0x40,0x35,0x82,
 0x00,0x87,0x89,0x08,0x00,0x00,0x00,0x41,0x57,0xa0,0x00,0xf4,
 0x4c,0x84,0x60,0x80,0x00,0x88,0x80,0xa0,0x00,0xff,0x02,0x80,
 0x6a,0xff,0x34,0x82,0x00,0x87,0x40,0x00,0x00,0x44,0x34,0x82,
 0x00,0x87,0x43,0x00,0x00,0x45,0x34,0x82,0x00,0x87,0x41,0x00,
 0x00,0x43,0x59,0x92,0x06,0x9c,0x20,0x80,0x40,0xa0,0x42,0x10,
 0x00,0x2c,0x40,0xff,0xf2,0xff,0xff,0xff,0x34,0x8a,0x80,0x87,
 0x40,0x00,0x00,0x20,
};
static int range(const struct geo_draw_span *s,uint32_t total)
{return s->bytes&&!(s->offset&3u)&&!(s->bytes&3u)&&s->offset<=total&&s->bytes<=total-s->offset;}
int32_t a04_m111_texture2d_program(const struct geo_draw_shader *s,const void *blob,uint32_t bytes)
{
 const uint8_t *b=blob;struct geo_sm4_signatures signature={0};struct geo_texture2d_program p;
 if(!s||!b||((uintptr_t)b&3u)||!range(&s->tokens,bytes)||!range(&s->usc,bytes)||!range(&s->constants,bytes)||s->constants.bytes!=512)return GEO_INVALID;
 if(s->format!=GEO_SHADER_D3D10_TOKENS||s->stage!=1||s->temps!=A04_TEXTURE2D_SAMPLE_TEMPS||s->shareds!=12||s->vtxins||s->vtxouts||s->input_mask||s->constant_mask!=1||s->usc.bytes!=sizeof(expected_sample_code))return GEO_UNSUPPORTED;
 signature.output_count=1;signature.outputs[0].mask=15;
 int32_t status=geo_sm4_texture2d_decode((const uint32_t *)(b+s->tokens.offset),s->tokens.bytes,&signature,&p);if(status)return status;
 if(p.coordinate_constant||p.coordinate_x||p.coordinate_y!=1)return GEO_UNSUPPORTED;
 for(uint32_t i=0;i<sizeof(expected_sample_code);i++)if(b[s->usc.offset+i]!=expected_sample_code[i])return GEO_UNSUPPORTED;
 return GEO_OK;
}
