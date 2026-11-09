/* SPDX-License-Identifier: MIT */
#include "typed_device.h"
#include "../memory.h"
static D3D10DDI_HDEVICE device(DXGI_DDI_HDEVICE value){D3D10DDI_HDEVICE h;h.pDrvPrivate=(void *)value;return h;}
static D3D10DDI_HRESOURCE resource(DXGI_DDI_HRESOURCE value){D3D10DDI_HRESOURCE h;h.pDrvPrivate=(void *)value;return h;}
static HRESULT APIENTRY priority(DXGI_DDI_ARG_SETRESOURCEPRIORITY *p) {
 D3D10DDI_HDEVICE h;struct geo_typed_device *d;struct geo_typed_resource *r;D3DKMT_HANDLE a;D3DDDICB_SETPRIORITY q;HRESULT hr;
 if(!p)return E_INVALIDARG;h=device(p->hDevice);d=geo_typed_device(h);r=geo_typed_resource(h,resource(p->hResource));
 if(!d||!r||!d->backend.cb.pfnSetPriorityCb)return E_INVALIDARG;
 hr=geo_typed_allocation(r,&a);if(FAILED(hr))return hr;memset(&q,0,sizeof(q));q.NumAllocations=1;q.HandleList=&a;q.pPriorities=&p->Priority;
 return d->backend.cb.pfnSetPriorityCb(d->backend.runtime,&q);
}
static HRESULT APIENTRY residency(DXGI_DDI_ARG_QUERYRESOURCERESIDENCY *p) {
 D3D10DDI_HDEVICE h;struct geo_typed_device *d;D3DKMT_HANDLE *handles;D3DDDI_RESIDENCYSTATUS *status;D3DDDICB_QUERYRESIDENCY q;HRESULT hr=S_OK;UINT i;
 if(!p||!p->Resources||p->Resources>UINT_MAX/sizeof(*handles)||!p->pResources||!p->pStatus)return E_INVALIDARG;
 h=device(p->hDevice);d=geo_typed_device(h);if(!d||!d->backend.cb.pfnQueryResidencyCb)return E_INVALIDARG;
 handles=HeapAlloc(GetProcessHeap(),0,p->Resources*sizeof(*handles));status=HeapAlloc(GetProcessHeap(),0,p->Resources*sizeof(*status));
 if(!handles||!status){if(handles)HeapFree(GetProcessHeap(),0,handles);if(status)HeapFree(GetProcessHeap(),0,status);return E_OUTOFMEMORY;}
 for(i=0;i<p->Resources;i++){struct geo_typed_resource *r=geo_typed_resource(h,resource(p->pResources[i]));if(!r){hr=E_INVALIDARG;break;}hr=geo_typed_allocation(r,&handles[i]);if(FAILED(hr))break;}
 if(SUCCEEDED(hr)){
  memset(&q,0,sizeof(q));q.NumAllocations=(UINT)p->Resources;q.HandleList=handles;q.pResidencyStatus=status;hr=d->backend.cb.pfnQueryResidencyCb(d->backend.runtime,&q);
  if(SUCCEEDED(hr))for(i=0;i<p->Resources;i++){
   if(status[i]==D3DDDI_RESIDENCYSTATUS_RESIDENTINGPUMEMORY)p->pStatus[i]=DXGI_DDI_RESIDENCY_FULLY_RESIDENT;
   else if(status[i]==D3DDDI_RESIDENCYSTATUS_RESIDENTINSHAREDMEMORY)p->pStatus[i]=DXGI_DDI_RESIDENCY_RESIDENT_IN_SHARED_MEMORY;
   else if(status[i]==D3DDDI_RESIDENCYSTATUS_NOTRESIDENT)p->pStatus[i]=DXGI_DDI_RESIDENCY_EVICTED_TO_DISK;
   else {hr=E_UNEXPECTED;break;}
  }
 }
 HeapFree(GetProcessHeap(),0,handles);HeapFree(GetProcessHeap(),0,status);return hr;
}
static HRESULT resource_list(D3D10DDI_HDEVICE h,const DXGI_DDI_HRESOURCE *in,UINT count,HANDLE **out) {
 HANDLE *list;UINT i;struct geo_typed_resource *r;
 if(!in||!count||count>UINT_MAX/sizeof(*list))return E_INVALIDARG;
 list=HeapAlloc(GetProcessHeap(),0,count*sizeof(*list));if(!list)return E_OUTOFMEMORY;
 for(i=0;i<count;i++){r=geo_typed_resource(h,resource(in[i]));if(!r){HeapFree(GetProcessHeap(),0,list);return E_INVALIDARG;}list[i]=r->backend;}
 *out=list;return S_OK;
}
static HRESULT APIENTRY offer(DXGI_DDI_ARG_OFFERRESOURCES *p) {
 D3D10DDI_HDEVICE h;struct geo_typed_device *d;D3DDDIARG_OFFERRESOURCES q;HANDLE *list;HRESULT hr;
 if(!p)return E_INVALIDARG;h=device(p->hDevice);d=geo_typed_device(h);if(!d)return E_INVALIDARG;
 hr=resource_list(h,p->pResources,p->Resources,&list);if(FAILED(hr))return hr;
 memset(&q,0,sizeof(q));q.pResources=list;q.Resources=p->Resources;q.Priority=p->Priority;hr=d->memory.pfnOfferResources(&d->backend,&q);HeapFree(GetProcessHeap(),0,list);return hr;
}
static HRESULT APIENTRY reclaim(DXGI_DDI_ARG_RECLAIMRESOURCES *p) {
 D3D10DDI_HDEVICE h;struct geo_typed_device *d;D3DDDIARG_RECLAIMRESOURCES q;HANDLE *list;HRESULT hr;
 if(!p)return E_INVALIDARG;h=device(p->hDevice);d=geo_typed_device(h);if(!d)return E_INVALIDARG;
 hr=resource_list(h,p->pResources,p->Resources,&list);if(FAILED(hr))return hr;
 memset(&q,0,sizeof(q));q.pResources=list;q.Resources=p->Resources;q.pDiscarded=p->pDiscarded;hr=d->memory.pfnReclaimResources(&d->backend,&q);HeapFree(GetProcessHeap(),0,list);return hr;
}
/* This adapter has no VidPn/display outputs, primary or shared-resource cap.
 * Optional display/sharing requests return a real unsupported result instead
 * of dereferencing NULL callbacks or pretending that content was presented. */
/* This is an OS presentation request, not a display completion. The runtime
 * retains the allocation references for accepted queued work. Private resource
 * addresses and POST physical addresses must never cross this callback ABI.
 * Keep C objects alive across a reentrant runtime callback; KMD owns subsequent
 * DMA/fence references. No CPU readback, wait-for-idle or synthetic completion. */
static HRESULT APIENTRY present(DXGI_DDI_ARG_PRESENT *p) {
 D3D10DDI_HDEVICE h;struct geo_typed_device *d;struct geo_typed_resource *src,*dst=NULL;
 DXGIDDICB_PRESENT q;HRESULT hr,cleanup;
 if(!p)return E_INVALIDARG;
 h=device(p->hDevice);d=geo_typed_device(h);if(!d)return E_INVALIDARG;
 if(FAILED(d->error))return d->error;
 if(!d->present_callback||!d->backend.frontend_flush||!d->backend.context)return DXGI_ERROR_UNSUPPORTED;
 /* No flip, broadcast, stereo, rotation or subresource support is advertised. */
 if(p->Flags.Value!=1u||p->FlipInterval!=DXGI_DDI_FLIP_INTERVAL_IMMEDIATE||
    p->SrcSubResourceIndex||p->DstSubResourceIndex)return DXGI_ERROR_UNSUPPORTED;
 if(d->presenting)return DXGI_ERROR_WAS_STILL_DRAWING;
 src=geo_typed_resource(h,resource(p->hSurfaceToPresent));
 if(!src||src->mapped||src->dimension!=D3D10DDIRESOURCE_TEXTURE2D||
    !(src->bind&D3D10_DDI_BIND_RENDER_TARGET)||src->format!=DXGI_FORMAT_B8G8R8A8_UNORM)return E_INVALIDARG;
 if(p->hDstResource){
  dst=geo_typed_resource(h,resource(p->hDstResource));
  if(!dst||dst==src||dst->mapped||dst->dimension!=src->dimension||dst->format!=src->format||
     dst->width!=src->width||dst->height!=src->height)return E_INVALIDARG;
 }
 d->presenting=1;geo_typed_resource_ref(src);if(dst)geo_typed_resource_ref(dst);
 memset(&q,0,sizeof(q));
 hr=d->backend.frontend_flush(&d->backend);
 if(SUCCEEDED(hr))hr=geo_typed_allocation(src,&q.hSrcAllocation);
 if(SUCCEEDED(hr)&&dst)hr=geo_typed_allocation(dst,&q.hDstAllocation);
 if(SUCCEEDED(hr)){
  if(!q.hSrcAllocation||(dst&&!q.hDstAllocation))hr=E_UNEXPECTED;
  else {q.hContext=d->backend.context;q.pDXGIContext=p->pDXGIContext;
   hr=d->present_callback(d->backend.runtime,&q);}
 }
 /* Never turn runtime failure (including NOT_SUPPORTED) into success. */
 if(dst){cleanup=geo_typed_resource_unref(dst);if(SUCCEEDED(hr)&&FAILED(cleanup))hr=cleanup;}
 cleanup=geo_typed_resource_unref(src);if(SUCCEEDED(hr)&&FAILED(cleanup))hr=cleanup;
 d->presenting=0;return hr;
}
static HRESULT APIENTRY gamma(DXGI_DDI_ARG_GET_GAMMA_CONTROL_CAPS *p){return p?DXGI_ERROR_UNSUPPORTED:E_INVALIDARG;}
static HRESULT APIENTRY display(DXGI_DDI_ARG_SETDISPLAYMODE *p){
 D3D10DDI_HDEVICE h;struct geo_typed_device *d;struct geo_typed_resource *r;D3DDDICB_SETDISPLAYMODE q;HRESULT hr;
 if(!p)return E_INVALIDARG;h=device(p->hDevice);d=geo_typed_device(h);
 if(!A04_GE8300_UNIFIED_DISPLAY356||!d||!d->backend.cb.pfnSetDisplayModeCb)return DXGI_ERROR_UNSUPPORTED;
 r=geo_typed_resource(h,resource(p->hResource));if(!r||!r->primary||r->mapped||p->SubResourceIndex)return E_INVALIDARG;
 if(FAILED(d->error))return d->error;memset(&q,0,sizeof(q));hr=geo_typed_allocation(r,&q.hPrimaryAllocation);
 if(FAILED(hr))return hr;return d->backend.cb.pfnSetDisplayModeCb(d->backend.runtime,&q);
}
static HRESULT APIENTRY rotate(DXGI_DDI_ARG_ROTATE_RESOURCE_IDENTITIES *p){
 struct geo_typed_resource *r[16];HANDLE first;UINT primary,i,j;D3D10DDI_HDEVICE h;struct geo_typed_device *d;D3DDDIARG_SETRENDERTARGET q;HRESULT hr;
 if(!p)return E_INVALIDARG;if(!A04_GE8300_UNIFIED_DISPLAY356)return DXGI_ERROR_UNSUPPORTED;
 h=device(p->hDevice);d=geo_typed_device(h);if(!d||!p->pResources||!p->Resources||p->Resources>16)return E_INVALIDARG;
 if(FAILED(d->error))return d->error;
 for(i=0;i<p->Resources;i++){
  r[i]=geo_typed_resource(h,resource(p->pResources[i]));if(!r[i]||r[i]->mapped)return E_INVALIDARG;
  for(j=0;j<i;j++)if(r[j]==r[i])return E_INVALIDARG;
  if(i&&(r[i]->width!=r[0]->width||r[i]->height!=r[0]->height||r[i]->format!=r[0]->format||r[i]->usage!=r[0]->usage||r[i]->bind!=r[0]->bind||r[i]->map_flags!=r[0]->map_flags))return DXGI_ERROR_UNSUPPORTED;
 }
 hr=d->backend.frontend_flush(&d->backend);if(FAILED(hr))return hr;
 first=r[0]->backend;primary=r[0]->primary;
 for(i=0;i+1<p->Resources;i++){r[i]->backend=r[i+1]->backend;r[i]->primary=r[i+1]->primary;}
 r[i]->backend=first;r[i]->primary=primary;
 if(d->target){memset(&q,0,sizeof(q));q.hRenderTarget=d->target->resource->backend;hr=d->memory.pfnSetRenderTarget(&d->backend,&q);}
 if(FAILED(hr)){
  first=r[p->Resources-1]->backend;primary=r[p->Resources-1]->primary;
  for(i=p->Resources-1;i;i--){r[i]->backend=r[i-1]->backend;r[i]->primary=r[i-1]->primary;}
  r[0]->backend=first;r[0]->primary=primary;
 }
 return hr;
}
static HRESULT APIENTRY blt(DXGI_DDI_ARG_BLT *p){
 D3D10DDI_HDEVICE h;struct geo_typed_device *d;struct geo_typed_resource *src,*dst;D3DDDIARG_BLT q;HRESULT hr,cleanup;
 if(!p)return E_INVALIDARG;if(!A04_GE8300_UNIFIED_DISPLAY356)return DXGI_ERROR_UNSUPPORTED;
 h=device(p->hDevice);d=geo_typed_device(h);if(!d)return E_INVALIDARG;if(FAILED(d->error))return d->error;
 if(p->SrcSubresource||p->DstSubresource||(p->Flags.Value&~8u)||p->Rotate!=DXGI_DDI_MODE_ROTATION_IDENTITY||p->DstLeft||p->DstTop)return DXGI_ERROR_UNSUPPORTED;
 src=geo_typed_resource(h,resource(p->hSrcResource));dst=geo_typed_resource(h,resource(p->hDstResource));
 if(!src||!dst||src==dst||src->mapped||dst->mapped||!dst->primary||!(src->bind&D3D10_DDI_BIND_RENDER_TARGET)||
    src->format!=DXGI_FORMAT_B8G8R8A8_UNORM||dst->format!=src->format||src->width!=dst->width||src->height!=dst->height||p->DstRight!=dst->width||p->DstBottom!=dst->height)return E_INVALIDARG;
 geo_typed_resource_ref(src);geo_typed_resource_ref(dst);memset(&q,0,sizeof(q));q.hSrcResource=src->backend;q.hDstResource=dst->backend;
 q.SrcRect.right=q.DstRect.right=src->width;q.SrcRect.bottom=q.DstRect.bottom=src->height;
 hr=d->backend.frontend_flush(&d->backend);if(SUCCEEDED(hr))hr=d->memory.pfnBlt(&d->backend,&q);
 cleanup=geo_typed_resource_unref(dst);if(SUCCEEDED(hr)&&FAILED(cleanup))hr=cleanup;
 cleanup=geo_typed_resource_unref(src);if(SUCCEEDED(hr)&&FAILED(cleanup))hr=cleanup;return hr;
}
static HRESULT APIENTRY resolve(DXGI_DDI_ARG_RESOLVESHAREDRESOURCE *p){return p?DXGI_ERROR_UNSUPPORTED:E_INVALIDARG;}
static HRESULT APIENTRY blt1(DXGI_DDI_ARG_BLT1 *p){
 DXGI_DDI_ARG_BLT q;struct geo_typed_resource *src;
 if(!p)return E_INVALIDARG;if(!A04_GE8300_UNIFIED_DISPLAY356)return DXGI_ERROR_UNSUPPORTED;
 src=geo_typed_resource(device(p->hDevice),resource(p->hSrcResource));
 if(!src||p->SrcLeft||p->SrcTop||p->SrcRight!=src->width||p->SrcBottom!=src->height)return DXGI_ERROR_UNSUPPORTED;
 memset(&q,0,sizeof(q));q.hDevice=p->hDevice;q.hSrcResource=p->hSrcResource;q.hDstResource=p->hDstResource;
 q.SrcSubresource=p->SrcSubresource;q.DstSubresource=p->DstSubresource;q.DstLeft=p->DstLeft;q.DstTop=p->DstTop;q.DstRight=p->DstRight;q.DstBottom=p->DstBottom;q.Flags=p->Flags;q.Rotate=p->Rotate;return blt(&q);
}
void geo_typed_dxgi_install(DXGI1_2_DDI_BASE_FUNCTIONS *f) {
 if(!f)return;memset(f,0,sizeof(*f));f->pfnPresent=present;f->pfnGetGammaCaps=gamma;f->pfnSetDisplayMode=display;f->pfnSetResourcePriority=priority;f->pfnQueryResourceResidency=residency;f->pfnRotateResourceIdentities=rotate;f->pfnBlt=blt;f->pfnResolveSharedResource=resolve;f->pfnBlt1=blt1;f->pfnOfferResources=offer;f->pfnReclaimResources=reclaim;
}
