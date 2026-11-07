/* SPDX-License-Identifier: MIT */
#include "shader_state.h"
#include "../memory.h"
struct shader {
 struct shader *next;struct geo_compiled_shader compiled;UINT deleted,token_bytes;
 /* Retain semantic input for kernel revalidation/compilation, not USC upload. */
 UINT tokens[1];
};
struct geo_umd_shader_state {
 struct geo_shader_compiler compiler;struct shader *list,*bound[2];
 UINT constants[2][GEO_SM2_CONSTANTS*4],valid[2];
};
static HRESULT flush(struct geo_umd_device *d){return d->frontend_flush?d->frontend_flush(d):E_UNEXPECTED;}
static struct shader *lookup(struct geo_umd_shader_state *s,HANDLE handle,UINT stage) {
 struct shader *p;for(p=s->list;p;p=p->next)if(p==(struct shader *)handle&&!p->deleted&&p->compiled.stage==stage)return p;return NULL;
}
static void collect(struct geo_umd_shader_state *s) {
 struct shader **link=&s->list,*p;
 while((p=*link)!=NULL) {
  if(p->deleted&&p!=s->bound[0]&&p!=s->bound[1]){*link=p->next;s->compiler.release(s->compiler.context,&p->compiled);HeapFree(GetProcessHeap(),0,p);}
  else link=&p->next;
 }
}
static HRESULT create(struct geo_umd_device *d,UINT stage,const UINT *tokens,UINT bytes,HANDLE *handle) {
 struct geo_umd_shader_state *s;struct shader *p;struct geo_sm2_error error;int32_t status;
 if(!d||!(s=d->shaders)||!tokens||!handle||bytes<8||(bytes&3)||bytes>GEO_SM2_MAX_TOKENS*4)return E_INVALIDARG;
 p=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*p)+bytes-sizeof(UINT));if(!p)return E_OUTOFMEMORY;
 memcpy(p->tokens,tokens,bytes);p->token_bytes=bytes;
 status=s->compiler.compile(s->compiler.context,stage,p->tokens,bytes,&p->compiled,&error);
 if(status!=GEO_OK){HeapFree(GetProcessHeap(),0,p);return status==GEO_UNSUPPORTED?E_NOTIMPL:E_INVALIDARG;}
 if(p->compiled.size!=sizeof(p->compiled)||p->compiled.version!=1||p->compiled.stage!=stage||
    !p->compiled.code||!p->compiled.code_bytes||!p->compiled.storage) {
  s->compiler.release(s->compiler.context,&p->compiled);HeapFree(GetProcessHeap(),0,p);return E_UNEXPECTED;
 }
 p->next=s->list;s->list=p;*handle=p;return S_OK;
}
static HRESULT APIENTRY create_vs(HANDLE h,D3DDDIARG_CREATEVERTEXSHADERFUNC *p,const UINT *code) {
 return p?create(h,0,code,p->Size,&p->ShaderHandle):E_INVALIDARG;
}
static HRESULT APIENTRY create_ps(HANDLE h,D3DDDIARG_CREATEPIXELSHADER *p,const UINT *code) {
 return p?create(h,1,code,p->CodeSize,&p->ShaderHandle):E_INVALIDARG;
}
static HRESULT bind_shader(struct geo_umd_device *d,UINT stage,HANDLE handle) {
 struct geo_umd_shader_state *s;struct shader *p=NULL;HRESULT hr;
 if(!d||!(s=d->shaders))return E_INVALIDARG;
 if(handle&&!(p=lookup(s,handle,stage)))return E_INVALIDARG;
 hr=flush(d);if(FAILED(hr))return hr;
 s->bound[stage]=p;collect(s);return S_OK;
}
static HRESULT APIENTRY bind_vs(HANDLE d,HANDLE p){return bind_shader(d,0,p);}
static HRESULT APIENTRY bind_ps(HANDLE d,HANDLE p){return bind_shader(d,1,p);}
static HRESULT delete_shader(struct geo_umd_device *d,UINT stage,HANDLE handle) {
 struct geo_umd_shader_state *s;struct shader *p;HRESULT hr;
 if(!d||!(s=d->shaders)||!(p=lookup(s,handle,stage)))return E_INVALIDARG;
 hr=flush(d);if(FAILED(hr))return hr;
 /* A deleted bound shader remains alive until replaced or device shutdown. */
 p->deleted=1;collect(s);return S_OK;
}
static HRESULT APIENTRY delete_vs(HANDLE d,HANDLE p){return delete_shader(d,0,p);}
static HRESULT APIENTRY delete_ps(HANDLE d,HANDLE p){return delete_shader(d,1,p);}
static HRESULT set_constants(struct geo_umd_device *d,UINT stage,UINT start,UINT count,const void *values) {
 struct geo_umd_shader_state *s;UINT i;HRESULT hr;
 if(!d||!(s=d->shaders)||!values||!count||start>=GEO_SM2_CONSTANTS||count>GEO_SM2_CONSTANTS-start)return E_INVALIDARG;
 hr=flush(d);if(FAILED(hr))return hr;
 /* Preserve exact float bit patterns. No FPU/unaligned float dereference. */
 memcpy(s->constants[stage]+start*4,values,count*4*sizeof(UINT));
 for(i=start;i<start+count;i++)s->valid[stage]|=1u<<i;return S_OK;
}
static HRESULT APIENTRY constants_vs(HANDLE d,const D3DDDIARG_SETVERTEXSHADERCONST *p,const void *v) {
 return p?set_constants(d,0,p->Register,p->Count,v):E_INVALIDARG;
}
static HRESULT APIENTRY constants_ps(HANDLE d,const D3DDDIARG_SETPIXELSHADERCONST *p,const FLOAT *v) {
 return p?set_constants(d,1,p->Register,p->Count,v):E_INVALIDARG;
}
HRESULT geo_umd_shader_init(struct geo_umd_device *d,const struct geo_shader_compiler *c) {
 struct geo_umd_shader_state *s;
 if(!d||d->shaders||!c||c->size!=sizeof(*c)||c->version!=1||!c->context||!c->compile||!c->release)return E_INVALIDARG;
 s=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*s));if(!s)return E_OUTOFMEMORY;
 s->compiler=*c;
 /* SM2 float constants initially zero; both banks have actual zero backing. */
 s->valid[0]=s->valid[1]=UINT32_MAX;d->shaders=s;return S_OK;
}
void geo_umd_install_shader_ddis(D3DDDI_DEVICEFUNCS *f) {
 if(!f)return;
 f->pfnCreateVertexShaderFunc=create_vs;f->pfnSetVertexShaderFunc=bind_vs;f->pfnDeleteVertexShaderFunc=delete_vs;
 f->pfnCreatePixelShader=create_ps;f->pfnSetPixelShader=bind_ps;f->pfnDeletePixelShader=delete_ps;
 f->pfnSetVertexShaderConst=constants_vs;f->pfnSetPixelShaderConst=constants_ps;
 /* Integer/bool/flow/texture/varying and draw DDIs are not success stubs. */
}
HRESULT geo_umd_shader_shutdown(struct geo_umd_device *d) {
 struct geo_umd_shader_state *s;struct shader *p;HRESULT hr;
 if(!d||!(s=d->shaders))return E_INVALIDARG;
 hr=flush(d);if(FAILED(hr))return hr;
 s->bound[0]=s->bound[1]=NULL;for(p=s->list;p;p=p->next)p->deleted=1;collect(s);
 HeapFree(GetProcessHeap(),0,s);d->shaders=NULL;return S_OK;
}
HRESULT geo_umd_shader_snapshot(struct geo_umd_device *d,UINT stage,const struct geo_compiled_shader **p,const UINT **constants,UINT *valid) {
 struct geo_umd_shader_state *s;struct shader *bound;
 if(!d||!(s=d->shaders)||stage>1||!p||!constants||!valid||!(bound=s->bound[stage]))return E_INVALIDARG;
 *p=&bound->compiled;*constants=s->constants[stage];*valid=s->valid[stage];return S_OK;
}

HRESULT geo_umd_shader_tokens(struct geo_umd_device *d,UINT stage,const UINT **tokens,UINT *bytes) {
 struct geo_umd_shader_state *s;struct shader *p;
 if(!d||!(s=d->shaders)||stage>1||!tokens||!bytes||!(p=s->bound[stage]))return E_INVALIDARG;
 *tokens=p->tokens;*bytes=p->token_bytes;return S_OK;
}
