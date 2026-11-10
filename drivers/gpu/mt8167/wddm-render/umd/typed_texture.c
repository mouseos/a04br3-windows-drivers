/* SPDX-License-Identifier: MIT
 * Owned BGRA8 single-mip Texture2D SRV; sampler objects/snapshot are owned by
 * typed_state.c. GPU memory and fence lifetime belong to VidMm/owner ABI3.
 */
#include "typed_device.h"
#include "typed_state.h"
#include "../memory.h"
#include "../frontend/texture_draw_abi.h"
struct geo_typed_srv {
 struct geo_typed_srv *next;struct geo_typed_device *device;
 struct geo_typed_resource *resource;UINT refs,deleted;
};
static struct geo_typed_srv *find(D3D10DDI_HDEVICE h,D3D10DDI_HSHADERRESOURCEVIEW handle) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_srv *v,*p=handle.pDrvPrivate?((struct geo_typed_handle *)handle.pDrvPrivate)->object:NULL;
 if(!d||!p)return NULL;for(v=d->texture_views;v&&v!=p;v=v->next){}return v&&!v->deleted?v:NULL;
}
static HRESULT unref(struct geo_typed_srv *v) {
 struct geo_typed_srv **p;HRESULT hr;if(!v||!v->refs)return E_INVALIDARG;if(--v->refs)return S_OK;
 hr=geo_typed_resource_unref(v->resource);if(FAILED(hr)){v->refs=1;return hr;}
 for(p=&v->device->texture_views;*p&&*p!=v;p=&(*p)->next){}if(!*p)return E_UNEXPECTED;
 *p=v->next;HeapFree(GetProcessHeap(),0,v);return S_OK;
}
static HRESULT validate(D3D10DDI_HDEVICE h,const D3D11DDIARG_CREATESHADERRESOURCEVIEW *p) {
 struct geo_typed_resource *r;if(!p||!(r=geo_typed_resource(h,p->hDrvResource)))return E_INVALIDARG;
 if(!(r->bind&D3D10_DDI_BIND_SHADER_RESOURCE)||r->dimension!=D3D10DDIRESOURCE_TEXTURE2D||p->ResourceDimension!=D3D10DDIRESOURCE_TEXTURE2D||
    (p->Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&p->Format!=DXGI_FORMAT_R8G8B8A8_UNORM)||p->Format!=r->format||p->Tex2D.MostDetailedMip||p->Tex2D.FirstArraySlice||p->Tex2D.MipLevels!=1||p->Tex2D.ArraySize!=1)return E_NOTIMPL;
 return S_OK;
}
static SIZE_T APIENTRY size(D3D10DDI_HDEVICE h,const D3D11DDIARG_CREATESHADERRESOURCEVIEW *p) {
 HRESULT hr=validate(h,p);if(FAILED(hr)){geo_typed_error(h,hr);return 0;}return sizeof(struct geo_typed_handle);
}
static void APIENTRY create(D3D10DDI_HDEVICE h,const D3D11DDIARG_CREATESHADERRESOURCEVIEW *p,D3D10DDI_HSHADERRESOURCEVIEW handle,D3D10DDI_HRTSHADERRESOURCEVIEW rt) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_srv *v;HRESULT hr;(void)rt;
 if(!d||!handle.pDrvPrivate){geo_typed_error(h,E_INVALIDARG);return;}((struct geo_typed_handle *)handle.pDrvPrivate)->object=NULL;
 hr=validate(h,p);if(FAILED(hr)){geo_typed_error(h,hr);return;}
 v=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*v));if(!v){geo_typed_error(h,E_OUTOFMEMORY);return;}
 v->refs=1;v->device=d;v->resource=geo_typed_resource(h,p->hDrvResource);geo_typed_resource_ref(v->resource);
 v->next=d->texture_views;d->texture_views=v;((struct geo_typed_handle *)handle.pDrvPrivate)->object=v;
}
static void APIENTRY destroy(D3D10DDI_HDEVICE h,D3D10DDI_HSHADERRESOURCEVIEW handle) {
 struct geo_typed_srv *v=find(h,handle);HRESULT hr;if(!v){geo_typed_error(h,E_INVALIDARG);return;}
 v->deleted=1;((struct geo_typed_handle *)handle.pDrvPrivate)->object=NULL;hr=unref(v);if(FAILED(hr))geo_typed_error(h,hr);
}
static void APIENTRY set(D3D10DDI_HDEVICE h,UINT first,UINT count,const D3D10DDI_HSHADERRESOURCEVIEW *handles) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_srv *v=NULL,*old;UINT i;HRESULT hr;
 if(!d||first>128||count>128-first||(count&&!handles)){geo_typed_error(h,E_INVALIDARG);return;}
 if(!count)return;
 for(i=0;i<count;i++)if(first+i&&handles[i].pDrvPrivate){geo_typed_error(h,E_NOTIMPL);return;}
 if(first)return;
 if(handles[0].pDrvPrivate&&!(v=find(h,handles[0]))){geo_typed_error(h,E_INVALIDARG);return;}
 if(v)v->refs++;old=d->texture;d->texture=v;if(old){hr=unref(old);if(FAILED(hr))geo_typed_error(h,hr);}
}
static void APIENTRY hazard(D3D10DDI_HDEVICE h,D3D10DDI_HSHADERRESOURCEVIEW handle,D3D10DDI_HRESOURCE resource) {
 struct geo_typed_srv *v=find(h,handle);struct geo_typed_resource *r=geo_typed_resource(h,resource);HRESULT hr;
 if(!v||r!=v->resource){geo_typed_error(h,E_INVALIDARG);return;}hr=geo_typed_wait_resource(r);if(FAILED(hr))geo_typed_error(h,hr);
}
HRESULT geo_typed_texture_profile(struct geo_typed_device *d,UINT *profile) {
 struct geo_typed_state_snapshot snapshot;const D3D10_DDI_SAMPLER_DESC *s;HRESULT hr;UINT f,a;
 if(!d||!profile)return E_INVALIDARG;
 hr=geo_typed_state_snapshot(d->state,&snapshot);if(FAILED(hr))return hr;s=&snapshot.sampler[GEO_TYPED_PS][0];
 /* NULL sampler is a real standard LINEAR+CLAMP default snapshot, not a
  * POINT substitution. All regular point/linear combinations + anisotropic
  * filtering have an actual SMP/native descriptor implementation. */
 f=(UINT)s->Filter;a=f==0x55u?s->MaxAnisotropy:1;
 if(s->AddressU!=D3D10_DDI_TEXTURE_ADDRESS_CLAMP||s->AddressV!=D3D10_DDI_TEXTURE_ADDRESS_CLAMP||s->AddressW!=D3D10_DDI_TEXTURE_ADDRESS_CLAMP||
    s->MipLODBias||s->MinLOD>0||s->MaxLOD<0)return E_NOTIMPL;
 *profile=GEO_TEXTURE_FILTER_PROFILE4|(f<<8)|a;
 if(!geo_texture_sampler_profile_valid(*profile)){*profile=0;return E_NOTIMPL;}return S_OK;
}
HRESULT geo_typed_texture_allocation(struct geo_typed_device *d,struct geo_surface_desc *desc,D3DKMT_HANDLE *a) {
 UINT profile;HRESULT hr;if(!d||!d->texture||!desc||!a)return E_INVALIDARG;
 if(d->target&&d->target->resource==d->texture->resource)return E_INVALIDARG;
 hr=geo_typed_texture_profile(d,&profile);if(FAILED(hr))return hr;
 return geo_umd_resource_info(&d->backend,d->texture->resource->backend,GEO_USAGE_TEXTURE2D,desc,a);
}
HRESULT geo_typed_texture_unbind(D3D10DDI_HDEVICE h) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_srv *v;if(!d)return E_INVALIDARG;v=d->texture;d->texture=NULL;return v?unref(v):S_OK;
}
void geo_typed_texture_install(D3D11_1DDI_DEVICEFUNCS *f) {
 f->pfnCalcPrivateShaderResourceViewSize=size;f->pfnCreateShaderResourceView=create;f->pfnDestroyShaderResourceView=destroy;
 f->pfnPsSetShaderResources=set;f->pfnShaderResourceViewReadAfterWriteHazard=hazard;
}
