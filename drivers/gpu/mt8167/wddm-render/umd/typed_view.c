/* SPDX-License-Identifier: MIT */
#include "typed_device.h"
void geo_typed_device_trace(struct geo_typed_device *,const char *,UINT);
#include "../memory.h"
static struct geo_typed_rtv *view(D3D10DDI_HDEVICE h,D3D10DDI_HRENDERTARGETVIEW v) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_rtv *r,*want=v.pDrvPrivate?((struct geo_typed_handle *)v.pDrvPrivate)->object:NULL;
 if(!d||!want)return NULL;for(r=d->views;r&&r!=want;r=r->next){}
 return r&&!r->deleted?r:NULL;
}
static HRESULT view_unref(struct geo_typed_rtv *v) {
 struct geo_typed_rtv **p;struct geo_typed_device *d;HRESULT hr;
 if(!v||!v->refs)return E_INVALIDARG;if(--v->refs)return S_OK;d=v->device;
 hr=geo_typed_resource_unref(v->resource);if(FAILED(hr)){v->refs=1;return hr;}
 for(p=&d->views;*p&&*p!=v;p=&(*p)->next){}if(!*p)return E_UNEXPECTED;*p=v->next;HeapFree(GetProcessHeap(),0,v);return S_OK;
}
static HRESULT view_desc(D3D10DDI_HDEVICE h,const D3D10DDIARG_CREATERENDERTARGETVIEW *p) {
 struct geo_typed_resource *r;
 if(!p||!(r=geo_typed_resource(h,p->hDrvResource)))return E_INVALIDARG;
 if(!(r->bind&D3D10_DDI_BIND_RENDER_TARGET)||p->Format!=r->format||p->ResourceDimension!=D3D10DDIRESOURCE_TEXTURE2D||p->Tex2D.MipSlice||p->Tex2D.FirstArraySlice||p->Tex2D.ArraySize!=1)return E_NOTIMPL;
 return S_OK;
}
static SIZE_T APIENTRY rtv_size(D3D10DDI_HDEVICE h,const D3D10DDIARG_CREATERENDERTARGETVIEW *p) {
 HRESULT hr=view_desc(h,p);if(FAILED(hr)){geo_typed_error(h,hr);return 0;}return sizeof(struct geo_typed_handle);
}
static void APIENTRY create_rtv(D3D10DDI_HDEVICE h,const D3D10DDIARG_CREATERENDERTARGETVIEW *p,D3D10DDI_HRENDERTARGETVIEW handle,D3D10DDI_HRTRENDERTARGETVIEW runtime) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_rtv *v;HRESULT hr;(void)runtime;
 if(!d||!handle.pDrvPrivate){geo_typed_error(h,E_INVALIDARG);return;}((struct geo_typed_handle *)handle.pDrvPrivate)->object=NULL;
 hr=view_desc(h,p);if(FAILED(hr)){geo_typed_error(h,hr);return;}
 v=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*v));if(!v){geo_typed_error(h,E_OUTOFMEMORY);return;}
 v->refs=1;v->device=d;v->resource=geo_typed_resource(h,p->hDrvResource);geo_typed_resource_ref(v->resource);
 v->next=d->views;d->views=v;((struct geo_typed_handle *)handle.pDrvPrivate)->object=v;
}
static void APIENTRY destroy_rtv(D3D10DDI_HDEVICE h,D3D10DDI_HRENDERTARGETVIEW handle) {
 struct geo_typed_rtv *v=view(h,handle);HRESULT hr;
 if(!v){geo_typed_error(h,E_INVALIDARG);return;}v->deleted=1;((struct geo_typed_handle *)handle.pDrvPrivate)->object=NULL;
 hr=view_unref(v);if(FAILED(hr))geo_typed_error(h,hr);
}
static void APIENTRY set_targets(D3D10DDI_HDEVICE h,const D3D10DDI_HRENDERTARGETVIEW *views,UINT count,UINT unbind,D3D10DDI_HDEPTHSTENCILVIEW depth,const D3D11DDI_HUNORDEREDACCESSVIEW *uav,const UINT *offsets,UINT first,UINT num,UINT changed,UINT changed_count) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_rtv *v=NULL,*old;D3DDDIARG_SETRENDERTARGET q;HRESULT hr;
 (void)unbind;(void)uav;(void)offsets;(void)first;(void)changed;(void)changed_count;
 if(!d||count>1||num||depth.pDrvPrivate||(count&&!views)){geo_typed_error(h,E_NOTIMPL);return;}
 if(count&&views[0].pDrvPrivate&&!(v=view(h,views[0]))){geo_typed_error(h,E_INVALIDARG);return;}
 memset(&q,0,sizeof(q));q.hRenderTarget=v?v->resource->backend:NULL;
 hr=d->memory.pfnSetRenderTarget(&d->backend,&q);if(FAILED(hr)){geo_typed_error(h,hr);return;}
 if(v)v->refs++;old=d->target;d->target=v;if(old){hr=view_unref(old);if(FAILED(hr))geo_typed_error(h,hr);}
}
static UINT unorm8(FLOAT f) {
 if(!(f>0.0f))return 0;if(f>=1.0f)return 255;return (UINT)(f*255.0f+0.5f);
}
static void APIENTRY clear_rtv(D3D10DDI_HDEVICE h,D3D10DDI_HRENDERTARGETVIEW handle,FLOAT color[4]) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_rtv *v=view(h,handle);D3DDDIARG_SETRENDERTARGET q;D3DDDIARG_CLEAR c;HRESULT hr,restore;
 if(!d||!v||!color){geo_typed_error(h,E_INVALIDARG);return;}
 memset(&q,0,sizeof(q));q.hRenderTarget=v->resource->backend;hr=d->memory.pfnSetRenderTarget(&d->backend,&q);
 if(SUCCEEDED(hr)){
  memset(&c,0,sizeof(c));c.Flags=D3DCLEAR_TARGET;c.FillColor=unorm8(color[2])|(unorm8(color[1])<<8)|(unorm8(color[0])<<16)|(unorm8(color[3])<<24);
  hr=d->memory.pfnClear(&d->backend,&c,0,NULL);geo_typed_device_trace(d,"CLEAR_RENDER_HRESULT",(UINT)hr);if(SUCCEEDED(hr))d->pending=1;
 }
 q.hRenderTarget=d->target?d->target->resource->backend:NULL;restore=d->memory.pfnSetRenderTarget(&d->backend,&q);
 if(SUCCEEDED(hr)&&FAILED(restore))hr=restore;if(FAILED(hr))geo_typed_error(h,hr);
}
static void APIENTRY clear_view(D3D10DDI_HDEVICE h,D3D11DDI_HANDLETYPE type,void *opaque,const FLOAT color[4],const D3D10_DDI_RECT *rects,UINT count) {
 D3D10DDI_HRENDERTARGETVIEW handle;struct geo_typed_rtv *v;FLOAT copy[4];
 if(type!=D3D10DDI_HT_RENDERTARGETVIEW||!color||count>1||(count&&!rects)){geo_typed_error(h,E_NOTIMPL);return;}
 handle.pDrvPrivate=opaque;v=view(h,handle);if(!v){geo_typed_error(h,E_INVALIDARG);return;}
 if(count&&(rects[0].left||rects[0].top||rects[0].right!=(LONG)v->resource->width||rects[0].bottom!=(LONG)v->resource->height)){geo_typed_error(h,E_NOTIMPL);return;}
 memcpy(copy,color,sizeof(copy));clear_rtv(h,handle,copy);
}
void geo_typed_views_install(D3D11_1DDI_DEVICEFUNCS *f) {
 f->pfnCalcPrivateRenderTargetViewSize=rtv_size;f->pfnCreateRenderTargetView=create_rtv;f->pfnDestroyRenderTargetView=destroy_rtv;
 f->pfnClearView=clear_view;f->pfnSetRenderTargets=set_targets;f->pfnClearRenderTargetView=clear_rtv;
}
HRESULT geo_typed_views_unbind(D3D10DDI_HDEVICE h) {
 struct geo_typed_device *d=geo_typed_device(h);D3DDDIARG_SETRENDERTARGET q;struct geo_typed_rtv *old;HRESULT hr;
 if(!d)return E_INVALIDARG;memset(&q,0,sizeof(q));hr=d->memory.pfnSetRenderTarget(&d->backend,&q);if(FAILED(hr))return hr;
 old=d->target;d->target=NULL;return old?view_unref(old):S_OK;
}
