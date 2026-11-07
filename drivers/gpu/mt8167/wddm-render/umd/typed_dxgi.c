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
static HRESULT APIENTRY present(DXGI_DDI_ARG_PRESENT *p){return p?DXGI_ERROR_UNSUPPORTED:E_INVALIDARG;}
static HRESULT APIENTRY gamma(DXGI_DDI_ARG_GET_GAMMA_CONTROL_CAPS *p){return p?DXGI_ERROR_UNSUPPORTED:E_INVALIDARG;}
static HRESULT APIENTRY display(DXGI_DDI_ARG_SETDISPLAYMODE *p){return p?DXGI_ERROR_UNSUPPORTED:E_INVALIDARG;}
static HRESULT APIENTRY rotate(DXGI_DDI_ARG_ROTATE_RESOURCE_IDENTITIES *p){return p?DXGI_ERROR_UNSUPPORTED:E_INVALIDARG;}
static HRESULT APIENTRY blt(DXGI_DDI_ARG_BLT *p){return p?DXGI_ERROR_UNSUPPORTED:E_INVALIDARG;}
static HRESULT APIENTRY resolve(DXGI_DDI_ARG_RESOLVESHAREDRESOURCE *p){return p?DXGI_ERROR_UNSUPPORTED:E_INVALIDARG;}
static HRESULT APIENTRY blt1(DXGI_DDI_ARG_BLT1 *p){return p?DXGI_ERROR_UNSUPPORTED:E_INVALIDARG;}
void geo_typed_dxgi_install(DXGI1_2_DDI_BASE_FUNCTIONS *f) {
 if(!f)return;memset(f,0,sizeof(*f));f->pfnPresent=present;f->pfnGetGammaCaps=gamma;f->pfnSetDisplayMode=display;f->pfnSetResourcePriority=priority;f->pfnQueryResourceResidency=residency;f->pfnRotateResourceIdentities=rotate;f->pfnBlt=blt;f->pfnResolveSharedResource=resolve;f->pfnBlt1=blt1;f->pfnOfferResources=offer;f->pfnReclaimResources=reclaim;
}
