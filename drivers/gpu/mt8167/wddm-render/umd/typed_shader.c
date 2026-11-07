/* SPDX-License-Identifier: MIT */
#include "typed_device.h"
#include "../memory.h"
static struct geo_typed_shader *shader(D3D10DDI_HDEVICE h,D3D10DDI_HSHADER handle) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_shader *s,*wanted=handle.pDrvPrivate?((struct geo_typed_handle *)handle.pDrvPrivate)->object:NULL;
 if(!d||!wanted)return NULL;for(s=d->shaders;s&&s!=wanted;s=s->next){}
 return s&&!s->deleted&&s->magic==GEO_TYPED_SHADER_MAGIC?s:NULL;
}
static void shader_unref(struct geo_typed_shader *s) {
 struct geo_typed_shader **p;
 if(!s||!s->refs||--s->refs)return;
 for(p=&s->device->shaders;*p&&*p!=s;p=&(*p)->next){}if(!*p)return;
 *p=s->next;geo_umd_release_tokenized_shader(s->code);s->magic=0;HeapFree(GetProcessHeap(),0,s);
}
static SIZE_T APIENTRY shader_size(D3D10DDI_HDEVICE h,const UINT *code,const D3D11_1DDIARG_STAGE_IO_SIGNATURES *signature) {
 if(!geo_typed_device(h)||!code||!signature||code[1]<3u||code[1]>GEO_SM2_MAX_TOKENS){geo_typed_error(h,E_INVALIDARG);return 0;}
 /* Allocation only: full program/signature is compiled by CreateShader,
  * which owns the resulting code until all binding refs have retired. */
 return sizeof(struct geo_typed_handle);
}
static void create_shader(D3D10DDI_HDEVICE h,const UINT *code,D3D10DDI_HSHADER handle,const D3D11_1DDIARG_STAGE_IO_SIGNATURES *signature,UINT stage) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_shader *s;HRESULT hr;
 if(!d||!handle.pDrvPrivate){geo_typed_error(h,E_INVALIDARG);return;}((struct geo_typed_handle *)handle.pDrvPrivate)->object=NULL;
 s=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*s));if(!s){geo_typed_error(h,E_OUTOFMEMORY);return;}
 hr=geo_umd_compile_shader11(&d->compiler,stage,code,signature,&s->code);
 if(FAILED(hr)){HeapFree(GetProcessHeap(),0,s);geo_typed_error(h,hr);return;}
 s->magic=GEO_TYPED_SHADER_MAGIC;s->refs=1;s->stage=stage;s->device=d;s->next=d->shaders;d->shaders=s;
 ((struct geo_typed_handle *)handle.pDrvPrivate)->object=s;
}
static void APIENTRY create_vs(D3D10DDI_HDEVICE h,const UINT *c,D3D10DDI_HSHADER s,D3D10DDI_HRTSHADER r,const D3D11_1DDIARG_STAGE_IO_SIGNATURES *sig){(void)r;create_shader(h,c,s,sig,0);}
static void APIENTRY create_ps(D3D10DDI_HDEVICE h,const UINT *c,D3D10DDI_HSHADER s,D3D10DDI_HRTSHADER r,const D3D11_1DDIARG_STAGE_IO_SIGNATURES *sig){(void)r;create_shader(h,c,s,sig,1);}
static void set_shader(D3D10DDI_HDEVICE h,D3D10DDI_HSHADER handle,UINT stage) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_shader *s=NULL,*old;
 if(!d){return;}if(handle.pDrvPrivate&&(!(s=shader(h,handle))||s->stage!=stage)){geo_typed_error(h,E_INVALIDARG);return;}
 if(s)s->refs++;old=d->bound_shader[stage];d->bound_shader[stage]=s;if(old)shader_unref(old);
}
static void APIENTRY set_vs(D3D10DDI_HDEVICE h,D3D10DDI_HSHADER s){set_shader(h,s,0);}
static void APIENTRY set_ps(D3D10DDI_HDEVICE h,D3D10DDI_HSHADER s){set_shader(h,s,1);}
static void APIENTRY destroy_shader(D3D10DDI_HDEVICE h,D3D10DDI_HSHADER handle) {
 struct geo_typed_shader *s=shader(h,handle);if(!s){geo_typed_error(h,E_INVALIDARG);return;}
 s->deleted=1;((struct geo_typed_handle *)handle.pDrvPrivate)->object=NULL;shader_unref(s);
}
static void constants(D3D10DDI_HDEVICE h,UINT start,UINT count,const D3D10DDI_HRESOURCE *handles,const UINT *first,const UINT *length,UINT stage) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_resource *r=NULL,*old;HRESULT hr;UINT at=count&&first?first[0]:0,n=count&&length?length[0]:0;
 if(!d||start||count>1u||(count&&!handles)){geo_typed_error(h,E_NOTIMPL);return;}
 if(count&&handles[0].pDrvPrivate){
  r=geo_typed_resource(h,handles[0]);
  if(r&&!length)n=r->bytes/16u<32u?r->bytes/16u:32u;
  if(!r||!(r->bind&D3D10_DDI_BIND_CONSTANT_BUFFER)||at>4096u||n>32u||at>r->bytes/16u||n>r->bytes/16u-at){geo_typed_error(h,E_INVALIDARG);return;}
 }
 if(r)geo_typed_resource_ref(r);old=d->constant[stage];d->constant[stage]=r;d->first_constant[stage]=at;d->constant_count[stage]=r?n:0;
 if(old){hr=geo_typed_resource_unref(old);if(FAILED(hr))geo_typed_error(h,hr);}
}
static void APIENTRY vs_constants(D3D10DDI_HDEVICE h,UINT a,UINT n,const D3D10DDI_HRESOURCE *r,const UINT *f,const UINT *c){constants(h,a,n,r,f,c,0);}
static void APIENTRY ps_constants(D3D10DDI_HDEVICE h,UINT a,UINT n,const D3D10DDI_HRESOURCE *r,const UINT *f,const UINT *c){constants(h,a,n,r,f,c,1);}
HRESULT geo_typed_shader_constants(struct geo_typed_device *d,UINT stage,UINT out[128]) {
 struct geo_typed_shader *s;UINT mask;HRESULT hr;
 if(!d||stage>1||!out||!(s=d->bound_shader[stage]))return E_INVALIDARG;
 memset(out,0,128u*sizeof(UINT));mask=s->code->compiled.constant_mask;
 if(!mask)return S_OK;
 if(!d->constant[stage]||!d->constant_count[stage]||
    (d->constant_count[stage]<32u&&(mask>>d->constant_count[stage])))return E_INVALIDARG;
 hr=geo_typed_read_resource(d->constant[stage],16u*d->first_constant[stage],16u*d->constant_count[stage],out);return hr;
}
HRESULT geo_typed_shaders_unbind(D3D10DDI_HDEVICE h) {
 struct geo_typed_device *d=geo_typed_device(h);UINT stage;HRESULT hr;if(!d)return E_INVALIDARG;
 for(stage=0;stage<2;stage++){
  struct geo_typed_shader *s=d->bound_shader[stage];struct geo_typed_resource *r=d->constant[stage];
  d->bound_shader[stage]=NULL;d->constant[stage]=NULL;if(s)shader_unref(s);
  if(r){hr=geo_typed_resource_unref(r);if(FAILED(hr))return hr;}
 }
 return S_OK;
}
void geo_typed_shaders_install(D3D11_1DDI_DEVICEFUNCS *f) {
 f->pfnCalcPrivateShaderSize=shader_size;f->pfnCreateVertexShader=create_vs;f->pfnCreatePixelShader=create_ps;
 f->pfnVsSetShader=set_vs;f->pfnPsSetShader=set_ps;f->pfnDestroyShader=destroy_shader;
 f->pfnVsSetConstantBuffers=vs_constants;f->pfnPsSetConstantBuffers=ps_constants;
}
