/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "clear_ddi.h"
#include "../memory.h"
struct geo_umd_resource {
 struct geo_umd_resource *next;
 struct geo_umd_device *device;
 HANDLE runtime;
 D3DKMT_HANDLE allocation;
 struct geo_surface_desc desc;
 UINT locked;
};
static struct geo_umd_resource *find(struct geo_umd_device *d,HANDLE h) {
 struct geo_umd_resource *r;for(r=d->resources;r;r=r->next)if(r==(struct geo_umd_resource *)h)return r;return NULL;
}
HRESULT geo_umd_clear_init(struct geo_umd_device *d,HANDLE runtime,const D3DDDI_DEVICECALLBACKS *cb,const D3DDDICB_CREATECONTEXT *c) {
 if(!d||!runtime||!cb||!c||!c->hContext||!cb->pfnAllocateCb||!cb->pfnDeallocateCb||!cb->pfnRenderCb||
    !cb->pfnLockCb||!cb->pfnUnlockCb||!c->pCommandBuffer||c->CommandBufferSize<sizeof(struct geo_clear_packet)||
    !c->pAllocationList||!c->AllocationListSize)return E_INVALIDARG;
 memset(d,0,sizeof(*d));d->runtime=runtime;d->cb=*cb;d->context=c->hContext;
 d->commands=c->pCommandBuffer;d->command_bytes=c->CommandBufferSize;
 d->allocations=c->pAllocationList;d->allocation_count=c->AllocationListSize;
 d->patches=c->pPatchLocationList;d->patch_count=c->PatchLocationListSize;return S_OK;
}
static HRESULT APIENTRY create_resource(HANDLE h,D3DDDIARG_CREATERESOURCE *p) {
 struct geo_umd_device *d=h;struct geo_umd_resource *r;D3DDDICB_ALLOCATE request;D3DDDI_ALLOCATIONINFO info;HRESULT hr;
 if(!d||!p||!p->pSurfList||p->SurfCount!=1||p->MipLevels!=1||p->Format!=D3DDDIFMT_A8R8G8B8||
    (p->Pool!=D3DDDIPOOL_SYSTEMMEM&&p->Pool!=D3DDDIPOOL_STAGINGMEM&&p->Pool!=D3DDDIPOOL_VIDEOMEMORY)||
    p->MultisampleType!=D3DDDIMULTISAMPLE_NONE||p->MultisampleQuality||p->pSurfList[0].Depth>1||p->pSurfList[0].pSysMem||
    (p->Flags.Value&~(1u|0x80u|0x10000u|0x1000000u))||p->Rotation!=D3DDDI_ROTATION_IDENTITY)return E_NOTIMPL;
 r=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*r));if(!r)return E_OUTOFMEMORY;
 r->device=d;r->runtime=p->hResource;r->desc.size=sizeof(r->desc);r->desc.version=1;
 r->desc.width=p->pSurfList[0].Width;r->desc.height=p->pSurfList[0].Height;
 if(r->desc.width>1024){HeapFree(GetProcessHeap(),0,r);return E_NOTIMPL;}
 r->desc.pitch=r->desc.width*4;r->desc.format=GEO_FORMAT_A8R8G8B8;
 r->desc.usage=p->Flags.RenderTarget?GEO_USAGE_RENDER_TARGET:GEO_USAGE_STAGING;
 if(!geo_desc_valid(&r->desc)){HeapFree(GetProcessHeap(),0,r);return E_INVALIDARG;}
 memset(&request,0,sizeof(request));memset(&info,0,sizeof(info));info.pPrivateDriverData=&r->desc;info.PrivateDriverDataSize=sizeof(r->desc);
 request.hResource=r->runtime;request.NumAllocations=1;request.pAllocationInfo=&info;
 hr=d->cb.pfnAllocateCb(d->runtime,&request);
 if(FAILED(hr)){HeapFree(GetProcessHeap(),0,r);return hr;}
 r->allocation=info.hAllocation;
 if(!r->allocation){/* Runtime/provider contract violation; retain ambiguous allocation. */return E_UNEXPECTED;}
 r->next=d->resources;d->resources=r;p->hResource=r;return S_OK;
}
static HRESULT APIENTRY destroy_resource(HANDLE h,HANDLE resource) {
 struct geo_umd_device *d=h;struct geo_umd_resource *r,**link;D3DDDICB_DEALLOCATE q;HRESULT hr;
 if(!d||!(r=find(d,resource)))return E_INVALIDARG;
 if(r->locked)return E_FAIL;
 memset(&q,0,sizeof(q));q.hResource=r->runtime;
 hr=d->cb.pfnDeallocateCb(d->runtime,&q);if(FAILED(hr))return hr;
 if(d->target==r)d->target=NULL;
 for(link=&d->resources;*link!=r;link=&(*link)->next){}*link=r->next;
 HeapFree(GetProcessHeap(),0,r);return S_OK;
}
static HRESULT APIENTRY set_target(HANDLE h,const D3DDDIARG_SETRENDERTARGET *p) {
 struct geo_umd_device *d=h;struct geo_umd_resource *r;
 if(!d||!p||p->RenderTargetIndex||p->SubResourceIndex)return E_INVALIDARG;
 if(!p->hRenderTarget){d->target=NULL;return S_OK;}
 r=find(d,p->hRenderTarget);if(!r||r->desc.usage!=GEO_USAGE_RENDER_TARGET||r->locked)return E_INVALIDARG;
 d->target=r;return S_OK;
}
static HRESULT APIENTRY clear(HANDLE h,const D3DDDIARG_CLEAR *p,UINT count,const RECT *rects) {
 struct geo_umd_device *d=h;struct geo_clear_packet packet;D3DDDICB_RENDER q;HRESULT hr;
 if(!d||!p||!d->target||d->target->locked)return E_INVALIDARG;
 if(p->Flags!=D3DCLEAR_TARGET||count>1)return E_NOTIMPL;
 if(count&&(!rects||rects[0].left||rects[0].top||rects[0].right!=(LONG)d->target->desc.width||rects[0].bottom!=(LONG)d->target->desc.height))return E_NOTIMPL;
 if(d->command_bytes<sizeof(packet)||!d->allocation_count||!d->commands||!d->allocations)return E_OUTOFMEMORY;
 memset(&packet,0,sizeof(packet));packet.magic=GEO_CLEAR_MAGIC;packet.size=sizeof(packet);packet.version=1;packet.color=p->FillColor;
 memcpy(d->commands,&packet,sizeof(packet));memset(&d->allocations[0],0,sizeof(d->allocations[0]));
 d->allocations[0].hAllocation=d->target->allocation;d->allocations[0].WriteOperation=1;
 memset(&q,0,sizeof(q));q.CommandLength=sizeof(packet);q.NumAllocations=1;q.hContext=d->context;
 hr=d->cb.pfnRenderCb(d->runtime,&q);if(FAILED(hr))return hr;
 d->commands=q.pNewCommandBuffer;d->command_bytes=q.NewCommandBufferSize;
 d->allocations=q.pNewAllocationList;d->allocation_count=q.NewAllocationListSize;
 d->patches=q.pNewPatchLocationList;d->patch_count=q.NewPatchLocationListSize;return S_OK;
}
static HRESULT APIENTRY lock_resource(HANDLE h,D3DDDIARG_LOCK *p) {
 struct geo_umd_device *d=h;struct geo_umd_resource *r;D3DDDICB_LOCK q;HRESULT hr;
 if(!d||!p||p->SubResourceIndex||!(r=find(d,p->hResource))||r->locked)return E_INVALIDARG;
 if(p->Flags.Value&~(1u|0x20u|0x200u))return E_NOTIMPL;
 if(p->Flags.AreaValid&&(p->Area.left||p->Area.top||p->Area.right!=(LONG)r->desc.width||p->Area.bottom!=(LONG)r->desc.height))return E_NOTIMPL;
 memset(&q,0,sizeof(q));q.hAllocation=r->allocation;q.Flags.ReadOnly=p->Flags.ReadOnly;q.Flags.DonotWait=p->Flags.DoNotWait;
 hr=d->cb.pfnLockCb(d->runtime,&q);if(FAILED(hr))return hr;
 if(!q.pData)return E_UNEXPECTED;
 r->allocation=q.hAllocation;r->locked=1;p->pSurfData=q.pData;p->Pitch=r->desc.pitch;p->SlicePitch=r->desc.pitch*r->desc.height;return S_OK;
}
static HRESULT APIENTRY unlock_resource(HANDLE h,const D3DDDIARG_UNLOCK *p) {
 struct geo_umd_device *d=h;struct geo_umd_resource *r;D3DDDICB_UNLOCK q;HRESULT hr;
 if(!d||!p||p->Flags.Value||p->SubResourceIndex||!(r=find(d,p->hResource))||!r->locked)return E_INVALIDARG;
 memset(&q,0,sizeof(q));q.NumAllocations=1;q.phAllocations=&r->allocation;
 hr=d->cb.pfnUnlockCb(d->runtime,&q);if(SUCCEEDED(hr))r->locked=0;return hr;
}
static HRESULT APIENTRY blt(HANDLE h,const D3DDDIARG_BLT *p) {
 struct geo_umd_device *d=h;struct geo_umd_resource *src,*dst;D3DDDIARG_LOCK a,b;D3DDDIARG_UNLOCK u;HRESULT hr,cleanup;UINT y;
 if(!d||!p||p->SrcSubResourceIndex||p->DstSubResourceIndex||p->Flags.Value>1)return E_NOTIMPL;
 src=find(d,p->hSrcResource);dst=find(d,p->hDstResource);
 if(!src||!dst||src==dst||src->desc.width!=dst->desc.width||src->desc.height!=dst->desc.height||
    dst->desc.usage!=GEO_USAGE_STAGING)return E_NOTIMPL;
 if(p->SrcRect.left||p->SrcRect.top||p->DstRect.left||p->DstRect.top||
    p->SrcRect.right!=(LONG)src->desc.width||p->SrcRect.bottom!=(LONG)src->desc.height||
    p->DstRect.right!=(LONG)dst->desc.width||p->DstRect.bottom!=(LONG)dst->desc.height)return E_NOTIMPL;
 memset(&a,0,sizeof(a));a.hResource=src;a.Flags.ReadOnly=1;
 hr=lock_resource(d,&a);if(FAILED(hr))return hr;
 memset(&b,0,sizeof(b));b.hResource=dst;hr=lock_resource(d,&b);
 if(SUCCEEDED(hr)) {
  /* Normal RAM required by owner ABI. Runtime lock waits for the REAL fence;
   * this copies GPU results for readback and never synthesizes a clear. */
  for(y=0;y<src->desc.height;y++)memcpy((BYTE *)b.pSurfData+y*b.Pitch,(BYTE *)a.pSurfData+y*a.Pitch,src->desc.width*4u);
  memset(&u,0,sizeof(u));u.hResource=dst;cleanup=unlock_resource(d,&u);if(FAILED(cleanup))hr=cleanup;
 }
 memset(&u,0,sizeof(u));u.hResource=src;cleanup=unlock_resource(d,&u);if(SUCCEEDED(hr)&&FAILED(cleanup))hr=cleanup;
 return hr;
}
void geo_umd_install_clear_ddis(D3DDDI_DEVICEFUNCS *f) {
 if(!f)return;
 f->pfnCreateResource=create_resource;f->pfnDestroyResource=destroy_resource;
 f->pfnBlt=blt;f->pfnSetRenderTarget=set_target;f->pfnClear=clear;f->pfnLock=lock_resource;f->pfnUnlock=unlock_resource;
}
