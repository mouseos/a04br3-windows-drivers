/* SPDX-License-Identifier: MIT */
#include "tokenized_shader.h"
#include "../compiler/pair_compiler.h"
#include "../compiler/texture2d_sm4.h"
#include "../clear_protocol.h"
#include "../memory.h"
static HRESULT compile(struct geo_shader_compiler *compiler,uint32_t stage,const UINT *code,
 const struct geo_sm4_signatures *signature,struct geo_umd_tokenized_shader **out) {
 struct geo_umd_tokenized_shader *s;struct geo_sm2_error error;uint32_t words;int32_t status;
 if(!out)return E_INVALIDARG;
 *out=NULL;
 if(!compiler||compiler->size!=sizeof(*compiler)||compiler->version!=1u||!compiler->context||!compiler->release||!code||stage>1u)return E_INVALIDARG;
 /* Runtime DDI contract supplies accessible code[0..code[1]). Never take a
  * DXBC container byte length or scan for SM2 END to bound this stream. */
 words=code[1];if(words<3u||words>GEO_SM2_MAX_TOKENS)return E_INVALIDARG;
 s=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*s));if(!s)return E_OUTOFMEMORY;
 s->compiler=compiler;s->token_bytes=words*4u;s->signatures=*signature;
 s->tokens=HeapAlloc(GetProcessHeap(),0,s->token_bytes);
 if(!s->tokens){HeapFree(GetProcessHeap(),0,s);return E_OUTOFMEMORY;}
 memcpy(s->tokens,code,s->token_bytes);
 status=geo_pco_compile_tokenized_witness(compiler->context,stage,s->tokens,s->token_bytes,&s->signatures,&s->compiled,&error,(void **)&s->witness,&s->witness_bytes);
 if(status!=GEO_OK)status=geo_pco_compile_tokenized(compiler->context,stage,s->tokens,s->token_bytes,&s->signatures,&s->compiled,&error);
 if(status!=GEO_OK&&stage==1u){struct geo_texture2d_program binding;status=geo_pco_compile_texture2d(compiler->context,s->tokens,s->token_bytes,&s->signatures,&s->compiled,&binding);if(status==GEO_OK)s->texture_profile=1;}
 if(status!=GEO_OK&&geo_pco_pair_validate_stage(stage,s->tokens,s->token_bytes,&s->signatures)==GEO_OK){
  s->pair_stage=stage+1u;s->compiled.stage=stage;s->compiled.input_mask=stage?1u:3u;status=GEO_OK;
 }
 if(status!=GEO_OK){geo_umd_release_tokenized_shader(s);return status==GEO_BUSY?E_OUTOFMEMORY:(status==GEO_UNSUPPORTED?E_NOTIMPL:E_INVALIDARG);}
 *out=s;return S_OK;
}
void geo_umd_release_tokenized_shader(struct geo_umd_tokenized_shader *s) {
 if(!s)return;
 if(s->premultiplied)geo_umd_release_tokenized_shader(s->premultiplied);
 if(s->compiled.storage)s->compiler->release(s->compiler->context,&s->compiled);
 if(s->tokens&&!s->is_variant)HeapFree(GetProcessHeap(),0,s->tokens);
 HeapFree(GetProcessHeap(),0,s);
}
HRESULT geo_umd_compile_shader10(struct geo_shader_compiler *compiler,uint32_t stage,const UINT *code,
 const D3D10DDIARG_STAGE_IO_SIGNATURES *in,struct geo_umd_tokenized_shader **out) {
 struct geo_sm4_signatures sig;uint32_t i;
 if(out)*out=NULL;
 if(!in||in->NumInputSignatureEntries>16u||in->NumOutputSignatureEntries>16u||
    (in->NumInputSignatureEntries&&!in->pInputSignature)||(in->NumOutputSignatureEntries&&!in->pOutputSignature))return E_INVALIDARG;
 memset(&sig,0,sizeof(sig));sig.input_count=in->NumInputSignatureEntries;sig.output_count=in->NumOutputSignatureEntries;
 for(i=0;i<sig.input_count;i++){sig.inputs[i].reg=in->pInputSignature[i].Register;sig.inputs[i].mask=in->pInputSignature[i].Mask;sig.inputs[i].system_value=in->pInputSignature[i].SystemValue;}
 for(i=0;i<sig.output_count;i++){sig.outputs[i].reg=in->pOutputSignature[i].Register;sig.outputs[i].mask=in->pOutputSignature[i].Mask;sig.outputs[i].system_value=in->pOutputSignature[i].SystemValue;}
 return compile(compiler,stage,code,&sig,out);
}
HRESULT geo_umd_compile_shader11(struct geo_shader_compiler *compiler,uint32_t stage,const UINT *code,
 const D3D11_1DDIARG_STAGE_IO_SIGNATURES *in,struct geo_umd_tokenized_shader **out) {
 struct geo_sm4_signatures sig;uint32_t i;
 if(out)*out=NULL;
 if(!in||in->NumInputSignatureEntries>16u||in->NumOutputSignatureEntries>16u||
    (in->NumInputSignatureEntries&&!in->pInputSignature)||(in->NumOutputSignatureEntries&&!in->pOutputSignature))return E_INVALIDARG;
 memset(&sig,0,sizeof(sig));sig.input_count=in->NumInputSignatureEntries;sig.output_count=in->NumOutputSignatureEntries;
 /* RT8.1 d3d11!CDevice::BuildDriverSignatures writes SystemValue, Register,
  * Mask and MinPrecision, but leaves RegisterComponentType untouched.
  * Do not read that unwritten field as a prerequisite to compilation. The
  * bounded token decoder still admits only the implemented float opcodes;
  * this does not add integer shader or minimum-precision support. */
 for(i=0;i<sig.input_count;i++){sig.inputs[i].reg=in->pInputSignature[i].Register;sig.inputs[i].mask=in->pInputSignature[i].Mask;sig.inputs[i].system_value=in->pInputSignature[i].SystemValue;
  if(in->pInputSignature[i].MinPrecision)return E_INVALIDARG;}
 for(i=0;i<sig.output_count;i++){sig.outputs[i].reg=in->pOutputSignature[i].Register;sig.outputs[i].mask=in->pOutputSignature[i].Mask;sig.outputs[i].system_value=in->pOutputSignature[i].SystemValue;
  if(in->pOutputSignature[i].MinPrecision)return E_INVALIDARG;}
 return compile(compiler,stage,code,&sig,out);
}

/* Immutable token/signature identity plus the one implemented blend mode.
 * Parent lifetime owns the variant; captured DMA copies own independent code. */
HRESULT geo_umd_premultiplied_variant(struct geo_umd_tokenized_shader *s,struct geo_umd_tokenized_shader **out) {
 struct geo_umd_tokenized_shader *v;int32_t status;struct geo_sm2_error error;
 if(!s||!out||s->compiled.stage!=1||s->pair_stage||s->is_variant)return E_INVALIDARG;
 if(s->premultiplied){*out=s->premultiplied;return S_OK;}
 v=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*v));if(!v)return E_OUTOFMEMORY;
 v->compiler=s->compiler;v->tokens=s->tokens;v->token_bytes=s->token_bytes;v->signatures=s->signatures;v->is_variant=1;v->texture_profile=s->texture_profile;
 if(s->texture_profile){struct geo_texture2d_program binding;
  status=geo_pco_compile_texture2d_blend(s->compiler->context,s->tokens,s->token_bytes,&s->signatures,&v->compiled,&binding,1);
 }else{void *proof=NULL;
  status=geo_pco_compile_tokenized_blend_witness(s->compiler->context,1,s->tokens,s->token_bytes,&s->signatures,&v->compiled,&error,&proof,&v->witness_bytes,1);v->witness=proof;
 }
 if(status){geo_umd_release_tokenized_shader(v);return status==GEO_BUSY?E_OUTOFMEMORY:E_NOTIMPL;}
 s->premultiplied=v;*out=v;return S_OK;
}
