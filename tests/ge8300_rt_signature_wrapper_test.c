/* SPDX-License-Identifier: MIT
 * Real WDK signatures + production wrapper/SM4 decoder. PCO code generation
 * is replaced only at its boundary; no GPU-code or runtime success claim. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../drivers/gpu/mt8167/wddm-render/umd/tokenized_shader.h"
#include "../drivers/gpu/mt8167/wddm-render/clear_protocol.h"
static unsigned decoder_calls;
int32_t geo_pco_compile_tokenized(void *context,uint32_t stage,const uint32_t *tokens,uint32_t bytes,
 const struct geo_sm4_signatures *signature,struct geo_compiled_shader *out,struct geo_sm2_error *error)
{
 struct geo_sm2_program decoded;int32_t result;assert(context);
 decoder_calls++;result=geo_sm4_decode(&decoded,stage,tokens,bytes,signature,error);
 if(result==GEO_OK){memset(out,0,sizeof(*out));out->stage=stage;}
 return result;
}
int32_t geo_pco_pair_validate_stage(uint32_t stage,const uint32_t *tokens,uint32_t bytes,const struct geo_sm4_signatures *signature)
{(void)stage;(void)tokens;(void)bytes;(void)signature;return GEO_UNSUPPORTED;}
static void release(void *context,struct geo_compiled_shader *shader)
{(void)context;(void)shader;assert(!"host boundary never allocates compiled GPU code");}
/* Fixed client geo_position_vs/geo_constant_ps SHDR tokens, not shader blobs. */
static const UINT vs[]={0x10040,0xf,0x300005f,0x1010f2,0,0x4000067,0x1020f2,0,1,0x5000036,0x1020f2,0,0x101e46,0,0x100003e};
static const UINT ps[]={0x40,0x10,0x4000059,0x208e46,0,1,0x3000065,0x1020f2,0,0x6000036,0x1020f2,0,0x208e46,0,0,0x100003e};
static void initialize_entry(D3D11_1DDIARG_SIGNATURE_ENTRY *e,unsigned poison,unsigned system)
{
 /* Match BuildDriverSignatures: initialize ONLY offsets0/4/8/16. Offset12
  * retains prior storage bytes and must not become a compilation predicate. */
 memset(e,(int)poison,sizeof(*e));e->SystemValue=system;e->Register=0;e->Mask=15;e->MinPrecision=0;
}
int main(void)
{
 struct geo_shader_compiler compiler={sizeof(compiler),1,&decoder_calls,NULL,release};
 D3D11_1DDIARG_SIGNATURE_ENTRY input,output;
 D3D11_1DDIARG_STAGE_IO_SIGNATURES signature;struct geo_umd_tokenized_shader *shader;
 HRESULT hr;unsigned i,before;UINT bad[sizeof(vs)/sizeof(*vs)];
 memset(&signature,0,sizeof(signature));signature.NumInputSignatureEntries=1;signature.pInputSignature=&input;
 signature.NumOutputSignatureEntries=1;signature.pOutputSignature=&output;
 for(i=0;i<3;i++){
  unsigned poison=i==0?0u:(i==1?0xa5u:0xffu);
  initialize_entry(&input,poison,0);initialize_entry(&output,poison,1);
  before=decoder_calls;hr=geo_umd_compile_shader11(&compiler,0,vs,&signature,&shader);
#ifdef GEO_EXPECT_OLD_TYPE_REJECTION
  assert(FAILED(hr) && !shader && decoder_calls==before);
#else
  assert(hr==S_OK && shader && decoder_calls==before+1 && shader->signatures.outputs[0].system_value==1);
  geo_umd_release_tokenized_shader(shader);
#endif
 }
 initialize_entry(&input,0xa5,0);initialize_entry(&output,0xa5,1);
 input.MinPrecision=1;before=decoder_calls;
 assert(FAILED(geo_umd_compile_shader11(&compiler,0,vs,&signature,&shader)) && !shader && decoder_calls==before);
 input.MinPrecision=0;output.MinPrecision=1;
 assert(FAILED(geo_umd_compile_shader11(&compiler,0,vs,&signature,&shader)) && !shader && decoder_calls==before);
 output.MinPrecision=0;memcpy(bad,vs,sizeof(bad));bad[9]=0x05000026u; /* IMAD unsupported, preserve length */
#ifdef GEO_EXPECT_OLD_TYPE_REJECTION
 assert(FAILED(geo_umd_compile_shader11(&compiler,0,bad,&signature,&shader)) && !shader && decoder_calls==before);
#else
 assert(FAILED(geo_umd_compile_shader11(&compiler,0,bad,&signature,&shader)) && !shader && decoder_calls==before+1);
#endif
 signature.NumInputSignatureEntries=0;initialize_entry(&output,0xff,0);
#ifdef GEO_EXPECT_OLD_TYPE_REJECTION
 assert(FAILED(geo_umd_compile_shader11(&compiler,1,ps,&signature,&shader)) && !shader);
#else
 assert(geo_umd_compile_shader11(&compiler,1,ps,&signature,&shader)==S_OK && shader);
 geo_umd_release_tokenized_shader(shader);
#endif
 signature.NumOutputSignatureEntries=17;
 assert(FAILED(geo_umd_compile_shader11(&compiler,1,ps,&signature,&shader)) && !shader);
#ifdef GEO_EXPECT_OLD_TYPE_REJECTION
 puts("Old wrapper reproduced unwritten type rejection for VS/PS; MODEL ONLY PASS");
#else
 puts("RT signature wrapper PASS: unwritten type poison0/A5/FF VS + PS accepted; both MinPrecision and unsupported IMAD rejected; real SM4 decoder exercised; PCO generation not tested");
#endif
 return 0;
}
