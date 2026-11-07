/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "typed_state.h"
#include <float.h>
#include <string.h>
/* Runtime owns each hState.pDrvPrivate allocation. Never HeapFree it. Runtime
 * serializes the immediate-context DDI; no deferred-context DDI is installed. */
enum object_kind { OBJ_RASTER = 1, OBJ_DEPTH, OBJ_BLEND, OBJ_SAMPLER };
union state_desc {
 D3D11_1_DDI_RASTERIZER_DESC raster;
 D3D10_DDI_DEPTH_STENCIL_DESC depth;
 D3D11_1_DDI_BLEND_DESC blend;
 D3D10_DDI_SAMPLER_DESC sampler;
};
struct state_object { struct state_object *next; UINT kind; union state_desc desc; };
struct geo_typed_state {
 struct state_object *objects;
 struct geo_typed_state_snapshot value;
};
static int finite_float(FLOAT x) { UINT u; memcpy(&u,&x,4); return (u&0x7f800000u)!=0x7f800000u; }
static int comparison(UINT x) { return x>=1u && x<=8u; }
static int blend_factor(UINT x) { return (x>=1u && x<=11u)||(x>=14u && x<=19u); }
static int alpha_factor(UINT x) {
 return blend_factor(x) && x!=3u && x!=4u && x!=9u && x!=10u && x!=16u && x!=17u;
}
static void default_raster(D3D11_1_DDI_RASTERIZER_DESC *d) {
 memset(d,0,sizeof(*d)); d->FillMode=D3D10_DDI_FILL_SOLID;
 d->CullMode=D3D10_DDI_CULL_BACK; d->DepthClipEnable=TRUE;
}
static void default_depth(D3D10_DDI_DEPTH_STENCIL_DESC *d) {
 memset(d,0,sizeof(*d)); d->DepthEnable=TRUE;
 d->DepthWriteMask=D3D10_DDI_DEPTH_WRITE_MASK_ALL; d->DepthFunc=D3D10_DDI_COMPARISON_LESS;
 d->FrontEnable=d->BackEnable=TRUE; d->StencilReadMask=d->StencilWriteMask=255;
 d->FrontFace.StencilFailOp=d->FrontFace.StencilDepthFailOp=d->FrontFace.StencilPassOp=D3D10_DDI_STENCIL_OP_KEEP;
 d->FrontFace.StencilFunc=D3D10_DDI_COMPARISON_ALWAYS; d->BackFace=d->FrontFace;
}
static void default_blend_target(D3D11_1_DDI_RENDER_TARGET_BLEND_DESC *d) {
 memset(d,0,sizeof(*d)); d->SrcBlend=d->SrcBlendAlpha=D3D10_DDI_BLEND_ONE;
 d->DestBlend=d->DestBlendAlpha=D3D10_DDI_BLEND_ZERO;
 d->BlendOp=d->BlendOpAlpha=D3D10_DDI_BLEND_OP_ADD;
 d->LogicOp=D3D11_1_DDI_LOGIC_OP_NOOP; d->RenderTargetWriteMask=15;
}
static void default_blend(D3D11_1_DDI_BLEND_DESC *d) {
 UINT i; memset(d,0,sizeof(*d)); for(i=0;i<8;i++)default_blend_target(&d->RenderTarget[i]);
}
static void default_sampler(D3D10_DDI_SAMPLER_DESC *d) {
 UINT i; memset(d,0,sizeof(*d)); d->Filter=D3D10_DDI_FILTER_MIN_MAG_MIP_LINEAR;
 d->AddressU=d->AddressV=d->AddressW=D3D10_DDI_TEXTURE_ADDRESS_CLAMP;
 d->MaxAnisotropy=1; d->ComparisonFunc=D3D10_DDI_COMPARISON_NEVER;
 for(i=0;i<4;i++)d->BorderColor[i]=1.0f;
 d->MinLOD=-FLT_MAX; d->MaxLOD=FLT_MAX;
}
HRESULT geo_typed_state_create(struct geo_typed_state **out) {
 struct geo_typed_state *s; UINT i,j;
 if(!out)return E_INVALIDARG; *out=NULL;
 s=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*s)); if(!s)return E_OUTOFMEMORY;
 s->value.size=sizeof(s->value); s->value.abi=GEO_TYPED_STATE_ABI;
 default_raster(&s->value.raster); default_depth(&s->value.depth); default_blend(&s->value.blend);
 s->value.sample_mask=0xffffffffu; for(i=0;i<4;i++)s->value.blend_factor[i]=1.0f;
 for(i=0;i<GEO_TYPED_STATE_STAGES;i++)for(j=0;j<GEO_TYPED_STATE_SAMPLERS;j++)default_sampler(&s->value.sampler[i][j]);
 *out=s; return S_OK;
}
void geo_typed_state_delete(struct geo_typed_state *s) {
 /* DestroyDevice invalidates the context; runtime allocations remain runtime
  * property. Normal runtime order calls DestroyState before DestroyDevice. */
 if(s) { memset(s,0,sizeof(*s)); HeapFree(GetProcessHeap(),0,s); }
}
HRESULT geo_typed_state_snapshot(const struct geo_typed_state *s,struct geo_typed_state_snapshot *out) {
 if(!s||!out)return E_INVALIDARG; *out=s->value; return S_OK;
}
static struct state_object *find(struct geo_typed_state *s,const void *p,UINT kind) {
 struct state_object *o; if(!s||!p)return NULL;
 /* Search by address before dereferencing a handle, including wrong-device
  * and destroyed handles. Bound descriptors are copies, never dangling refs. */
 for(o=s->objects;o;o=o->next)if((const void *)o==p)return o->kind==kind?o:NULL;
 return NULL;
}
static void create_object(D3D10DDI_HDEVICE h,void *p,UINT kind,const union state_desc *d,HRESULT hr) {
 struct geo_typed_state *s=geo_typed_state_from_device(h); struct state_object *o;
 if(!s||!p)hr=E_INVALIDARG;
 if(SUCCEEDED(hr)) { for(o=s->objects;o;o=o->next)if((void *)o==p){hr=E_INVALIDARG;break;} }
 if(FAILED(hr)){geo_typed_error(h,hr);return;}
 o=p; memset(o,0,sizeof(*o)); o->kind=kind; o->desc=*d; o->next=s->objects; s->objects=o;
}
static void destroy_object(D3D10DDI_HDEVICE h,void *p,UINT kind) {
 struct geo_typed_state *s=geo_typed_state_from_device(h); struct state_object **q;
 if(s&&p)for(q=&s->objects;*q;q=&(*q)->next)if((void *)*q==p&&(*q)->kind==kind) {
  struct state_object *o=*q; *q=o->next; memset(o,0,sizeof(*o)); return;
 }
 geo_typed_error(h,E_INVALIDARG);
}
static HRESULT raster_desc(const D3D11_1_DDI_RASTERIZER_DESC *in,D3D11_1_DDI_RASTERIZER_DESC *d) {
 UINT n; if(!in)return E_INVALIDARG; *d=*in;
 if((d->FillMode!=2&&d->FillMode!=3)||d->CullMode<1||d->CullMode>3||
    !finite_float(d->DepthBiasClamp)||!finite_float(d->SlopeScaledDepthBias))return E_INVALIDARG;
 n=d->ForcedSampleCount; if(n!=0&&n!=1&&n!=2&&n!=4&&n!=8&&n!=16)return E_INVALIDARG;
 d->FrontCounterClockwise=!!d->FrontCounterClockwise; d->DepthClipEnable=!!d->DepthClipEnable;
 d->ScissorEnable=!!d->ScissorEnable; d->MultisampleEnable=!!d->MultisampleEnable;
 d->AntialiasedLineEnable=!!d->AntialiasedLineEnable; return S_OK;
}
static int stencil_face(const D3D10_DDI_DEPTH_STENCILOP_DESC *f) {
 return comparison(f->StencilFailOp)&&comparison(f->StencilDepthFailOp)&&comparison(f->StencilPassOp)&&comparison(f->StencilFunc);
}
static HRESULT depth_desc(const D3D10_DDI_DEPTH_STENCIL_DESC *in,D3D10_DDI_DEPTH_STENCIL_DESC *d) {
 D3D10_DDI_DEPTH_STENCIL_DESC def; if(!in)return E_INVALIDARG; memset(d,0,sizeof(*d)); default_depth(&def);
 d->DepthEnable=in->DepthEnable; d->DepthWriteMask=in->DepthWriteMask; d->DepthFunc=in->DepthFunc;
 d->StencilEnable=in->StencilEnable; d->FrontEnable=in->FrontEnable; d->BackEnable=in->BackEnable;
 d->StencilReadMask=in->StencilReadMask; d->StencilWriteMask=in->StencilWriteMask;
 d->FrontFace=in->FrontFace; d->BackFace=in->BackFace;
 d->DepthEnable=!!d->DepthEnable; d->StencilEnable=!!d->StencilEnable;
 d->FrontEnable=!!d->FrontEnable; d->BackEnable=!!d->BackEnable;
 if(d->DepthEnable) { if((UINT)d->DepthWriteMask>1||!comparison(d->DepthFunc))return E_INVALIDARG; }
 else { d->DepthWriteMask=def.DepthWriteMask; d->DepthFunc=def.DepthFunc; }
 if(d->StencilEnable) {
  if(d->FrontEnable) { if(!stencil_face(&d->FrontFace))return E_INVALIDARG; } else d->FrontFace=def.FrontFace;
  if(d->BackEnable) { if(!stencil_face(&d->BackFace))return E_INVALIDARG; } else d->BackFace=def.BackFace;
 } else { d->FrontEnable=d->BackEnable=TRUE; d->FrontFace=def.FrontFace; d->BackFace=def.BackFace; }
 return S_OK;
}
static HRESULT blend_desc(const D3D11_1_DDI_BLEND_DESC *in,D3D11_1_DDI_BLEND_DESC *d) {
 UINT i,n; if(!in)return E_INVALIDARG; memset(d,0,sizeof(*d));
 d->AlphaToCoverageEnable=!!in->AlphaToCoverageEnable; d->IndependentBlendEnable=!!in->IndependentBlendEnable;
 n=d->IndependentBlendEnable?8u:1u;
 for(i=0;i<n;i++) {
  const D3D11_1_DDI_RENDER_TARGET_BLEND_DESC *a=&in->RenderTarget[i];
  D3D11_1_DDI_RENDER_TARGET_BLEND_DESC *b=&d->RenderTarget[i]; default_blend_target(b);
  if(a->RenderTargetWriteMask&~15u)return E_INVALIDARG;
  b->RenderTargetWriteMask=a->RenderTargetWriteMask; b->BlendEnable=!!a->BlendEnable; b->LogicOpEnable=!!a->LogicOpEnable;
  if(b->BlendEnable&&b->LogicOpEnable)return E_INVALIDARG;
  if(b->BlendEnable) {
   if(!blend_factor(a->SrcBlend)||!blend_factor(a->DestBlend)||!alpha_factor(a->SrcBlendAlpha)||!alpha_factor(a->DestBlendAlpha)||
      a->BlendOp<1||a->BlendOp>5||a->BlendOpAlpha<1||a->BlendOpAlpha>5)return E_INVALIDARG;
   b->SrcBlend=a->SrcBlend;b->DestBlend=a->DestBlend;b->BlendOp=a->BlendOp;
   b->SrcBlendAlpha=a->SrcBlendAlpha;b->DestBlendAlpha=a->DestBlendAlpha;b->BlendOpAlpha=a->BlendOpAlpha;
  }
  if(b->LogicOpEnable) { if((UINT)a->LogicOp>15u)return E_INVALIDARG; b->LogicOp=a->LogicOp; }
 }
 if(!d->IndependentBlendEnable)for(i=1;i<8;i++)d->RenderTarget[i]=d->RenderTarget[0];
 return S_OK;
}
static HRESULT sampler_desc(const D3D10_DDI_SAMPLER_DESC *in,D3D10_DDI_SAMPLER_DESC *d) {
 UINT f,i,base; if(!in)return E_INVALIDARG; *d=*in; f=(UINT)d->Filter;
 /* Standard/comparison point/linear combinations and anisotropic. Reduction
  * minimum/maximum and text-1bit are not part of this FL9_1 state contract. */
 base=f&~0x80u;
 if(!((base&~0x15u)==0u||base==0x55u))return E_INVALIDARG;
 if(d->AddressU<1||d->AddressU>5||d->AddressV<1||d->AddressV>5||d->AddressW<1||d->AddressW>5||
    !finite_float(d->MipLODBias)||d->MipLODBias < -16.0f||d->MipLODBias>15.99f||
    !finite_float(d->MinLOD)||!finite_float(d->MaxLOD)||d->MinLOD>d->MaxLOD)return E_INVALIDARG;
 if(base==0x55u) { if(d->MaxAnisotropy<1||d->MaxAnisotropy>16)return E_INVALIDARG; } else d->MaxAnisotropy=1;
 if(f&0x80u) { if(!comparison(d->ComparisonFunc))return E_INVALIDARG; } else d->ComparisonFunc=D3D10_DDI_COMPARISON_NEVER;
 for(i=0;i<4;i++)if(!finite_float(d->BorderColor[i]))return E_INVALIDARG;
 return S_OK;
}
#define STATE_DDI(name,desc_type,handle_type,rt_type,kind,member,normalize) \
 static SIZE_T APIENTRY calc_##name(D3D10DDI_HDEVICE h,const desc_type *d) { (void)h;(void)d;return sizeof(struct state_object); } \
 static void APIENTRY create_##name(D3D10DDI_HDEVICE h,const desc_type *d,handle_type o,rt_type rt) { \
  union state_desc v; HRESULT hr; (void)rt; memset(&v,0,sizeof(v));hr=normalize(d,&v.member);create_object(h,o.pDrvPrivate,kind,&v,hr); } \
 static void APIENTRY destroy_##name(D3D10DDI_HDEVICE h,handle_type o) {destroy_object(h,o.pDrvPrivate,kind);}
STATE_DDI(raster,D3D11_1_DDI_RASTERIZER_DESC,D3D10DDI_HRASTERIZERSTATE,D3D10DDI_HRTRASTERIZERSTATE,OBJ_RASTER,raster,raster_desc)
STATE_DDI(depth,D3D10_DDI_DEPTH_STENCIL_DESC,D3D10DDI_HDEPTHSTENCILSTATE,D3D10DDI_HRTDEPTHSTENCILSTATE,OBJ_DEPTH,depth,depth_desc)
STATE_DDI(blend,D3D11_1_DDI_BLEND_DESC,D3D10DDI_HBLENDSTATE,D3D10DDI_HRTBLENDSTATE,OBJ_BLEND,blend,blend_desc)
STATE_DDI(sampler,D3D10_DDI_SAMPLER_DESC,D3D10DDI_HSAMPLER,D3D10DDI_HRTSAMPLER,OBJ_SAMPLER,sampler,sampler_desc)
static void APIENTRY set_raster(D3D10DDI_HDEVICE h,D3D10DDI_HRASTERIZERSTATE a) {
 struct geo_typed_state *s=geo_typed_state_from_device(h); struct state_object *o=find(s,a.pDrvPrivate,OBJ_RASTER);
 if(!s||(a.pDrvPrivate&&!o)){geo_typed_error(h,E_INVALIDARG);return;}
 if(o)s->value.raster=o->desc.raster;else default_raster(&s->value.raster);
}
static void APIENTRY set_depth(D3D10DDI_HDEVICE h,D3D10DDI_HDEPTHSTENCILSTATE a,UINT ref) {
 struct geo_typed_state *s=geo_typed_state_from_device(h); struct state_object *o=find(s,a.pDrvPrivate,OBJ_DEPTH);
 if(!s||(a.pDrvPrivate&&!o)){geo_typed_error(h,E_INVALIDARG);return;}
 if(o)s->value.depth=o->desc.depth;else default_depth(&s->value.depth);
 s->value.stencil_ref=ref&255u;
}
static void APIENTRY set_blend(D3D10DDI_HDEVICE h,D3D10DDI_HBLENDSTATE a,const FLOAT factor[4],UINT mask) {
 UINT i; FLOAT f[4]; struct geo_typed_state *s=geo_typed_state_from_device(h); struct state_object *o=find(s,a.pDrvPrivate,OBJ_BLEND);
 if(!s||(a.pDrvPrivate&&!o)){geo_typed_error(h,E_INVALIDARG);return;}
 for(i=0;i<4;i++) { FLOAT x=factor?factor[i]:1.0f; if(!finite_float(x)){geo_typed_error(h,E_INVALIDARG);return;} f[i]=x<0?0:x>1?1:x; }
 if(o)s->value.blend=o->desc.blend;else default_blend(&s->value.blend);
 memcpy(s->value.blend_factor,f,sizeof(f)); s->value.sample_mask=mask;
}
static void set_samplers(D3D10DDI_HDEVICE h,UINT start,UINT n,const D3D10DDI_HSAMPLER *a,UINT stage) {
 UINT i; struct geo_typed_state *s=geo_typed_state_from_device(h);
 if(!s||start>16u||n>16u-start||(n&&!a)){geo_typed_error(h,E_INVALIDARG);return;}
 /* Validate the entire update before mutating any slot. */
 for(i=0;i<n;i++)if(a[i].pDrvPrivate&&!find(s,a[i].pDrvPrivate,OBJ_SAMPLER)){geo_typed_error(h,E_INVALIDARG);return;}
 for(i=0;i<n;i++) {
  struct state_object *o=find(s,a[i].pDrvPrivate,OBJ_SAMPLER); UINT bit=1u<<(start+i);
  if(o) {s->value.sampler[stage][start+i]=o->desc.sampler;s->value.sampler_bound_mask[stage]|=bit;}
  else {default_sampler(&s->value.sampler[stage][start+i]);s->value.sampler_bound_mask[stage]&=~bit;}
 }
}
#define SAMPLERS(name,stage) static void APIENTRY name(D3D10DDI_HDEVICE h,UINT start,UINT n,const D3D10DDI_HSAMPLER *a) {set_samplers(h,start,n,a,stage);}
SAMPLERS(set_ps,GEO_TYPED_PS) SAMPLERS(set_vs,GEO_TYPED_VS) SAMPLERS(set_gs,GEO_TYPED_GS)
SAMPLERS(set_hs,GEO_TYPED_HS) SAMPLERS(set_ds,GEO_TYPED_DS) SAMPLERS(set_cs,GEO_TYPED_CS)
void geo_typed_state_install(D3D11_1DDI_DEVICEFUNCS *f) {
 if(!f)return;
 f->pfnCalcPrivateRasterizerStateSize=calc_raster;f->pfnCreateRasterizerState=create_raster;f->pfnDestroyRasterizerState=destroy_raster;
 f->pfnCalcPrivateDepthStencilStateSize=calc_depth;f->pfnCreateDepthStencilState=create_depth;f->pfnDestroyDepthStencilState=destroy_depth;
 f->pfnCalcPrivateBlendStateSize=calc_blend;f->pfnCreateBlendState=create_blend;f->pfnDestroyBlendState=destroy_blend;
 f->pfnCalcPrivateSamplerSize=calc_sampler;f->pfnCreateSampler=create_sampler;f->pfnDestroySampler=destroy_sampler;
 f->pfnSetRasterizerState=set_raster;f->pfnSetDepthStencilState=set_depth;f->pfnSetBlendState=set_blend;
 f->pfnPsSetSamplers=set_ps;f->pfnVsSetSamplers=set_vs;f->pfnGsSetSamplers=set_gs;
 f->pfnHsSetSamplers=set_hs;f->pfnDsSetSamplers=set_ds;f->pfnCsSetSamplers=set_cs;
}
HRESULT geo_typed_state_validate_flat(const struct geo_typed_state_snapshot *s,const UINT used[6],BOOL dsv,UINT *unsupported) {
 UINT i,bits=0; const D3D11_1_DDI_RASTERIZER_DESC *r; const D3D11_1_DDI_RENDER_TARGET_BLEND_DESC *b;
 if(unsupported)*unsupported=0;
 if(!s||s->size!=sizeof(*s)||s->abi!=GEO_TYPED_STATE_ABI||!used||!unsupported)return E_INVALIDARG;
 r=&s->raster;b=&s->blend.RenderTarget[0];
 if(r->FillMode!=D3D10_DDI_FILL_SOLID||r->CullMode!=D3D10_DDI_CULL_NONE||r->FrontCounterClockwise||
    r->DepthBias||r->DepthBiasClamp!=0||r->SlopeScaledDepthBias!=0||!r->DepthClipEnable||r->ScissorEnable||
    r->MultisampleEnable||r->AntialiasedLineEnable||r->ForcedSampleCount)bits|=GEO_TYPED_FLAT_RASTER;
 /* No DSV means depth/stencil operations have no storage/effect; an enabled
  * default depth state alone must not spuriously reject a no-depth draw. */
 if(dsv&&(s->depth.DepthEnable||s->depth.StencilEnable))bits|=GEO_TYPED_FLAT_DEPTH;
 if(s->blend.AlphaToCoverageEnable||b->LogicOpEnable||b->RenderTargetWriteMask!=15u)bits|=GEO_TYPED_FLAT_BLEND;
 if(b->BlendEnable&&(b->SrcBlend!=D3D10_DDI_BLEND_ONE||b->DestBlend!=D3D10_DDI_BLEND_ZERO||b->BlendOp!=D3D10_DDI_BLEND_OP_ADD||
    b->SrcBlendAlpha!=D3D10_DDI_BLEND_ONE||b->DestBlendAlpha!=D3D10_DDI_BLEND_ZERO||b->BlendOpAlpha!=D3D10_DDI_BLEND_OP_ADD))bits|=GEO_TYPED_FLAT_BLEND;
 if(s->sample_mask!=0xffffffffu)bits|=GEO_TYPED_FLAT_SAMPLES;
 for(i=0;i<6;i++) {if(used[i]&~0xffffu)return E_INVALIDARG;if(used[i])bits|=GEO_TYPED_FLAT_TEXTURE;}
 *unsupported=bits; return bits?DXGI_ERROR_UNSUPPORTED:S_OK;
}
