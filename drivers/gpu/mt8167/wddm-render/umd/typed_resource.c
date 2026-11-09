/* SPDX-License-Identifier: MIT */
#include "typed_device.h"
void geo_typed_device_trace(struct geo_typed_device *,const char *,UINT);
#include "../memory.h"
/* Internal development trace only; no resource/handle ABI change. Log the
 * real runtime descriptor before validation so the staging failure is not
 * confused with KMD allocation failure or a rejected Map operation. */
void geo_typed_resource_trace(D3D10DDI_HDEVICE,const char *,UINT);
static void trace_descriptor(D3D10DDI_HDEVICE h,UINT operation,const D3D11DDIARG_CREATERESOURCE *p) {
 geo_typed_resource_trace(h,"RESOURCE_OPERATION",operation);
 if(!p){geo_typed_resource_trace(h,"RESOURCE_NULL_DESCRIPTOR",1);return;}
 geo_typed_resource_trace(h,"RESOURCE_DIMENSION",p->ResourceDimension);
 geo_typed_resource_trace(h,"RESOURCE_USAGE",p->Usage);
 geo_typed_resource_trace(h,"RESOURCE_BIND",p->BindFlags);
 geo_typed_resource_trace(h,"RESOURCE_MAP_FLAGS",p->MapFlags);
 geo_typed_resource_trace(h,"RESOURCE_MISC",p->MiscFlags);
 geo_typed_resource_trace(h,"RESOURCE_FORMAT",p->Format);
 geo_typed_resource_trace(h,"RESOURCE_BYTE_STRIDE",p->ByteStride);
 geo_typed_resource_trace(h,"RESOURCE_DECODER_TYPE",p->DecoderBufferType);
 geo_typed_resource_trace(h,"RESOURCE_MIP_LEVELS",p->MipLevels);
 geo_typed_resource_trace(h,"RESOURCE_ARRAY_SIZE",p->ArraySize);
 geo_typed_resource_trace(h,"RESOURCE_SAMPLES",p->SampleDesc.Count);
 geo_typed_resource_trace(h,"RESOURCE_SAMPLE_QUALITY",p->SampleDesc.Quality);
 if(p->pMipInfoList){
  const D3D10DDI_MIPINFO *m=p->pMipInfoList;
  geo_typed_resource_trace(h,"RESOURCE_TEXEL_WIDTH",m->TexelWidth);
  geo_typed_resource_trace(h,"RESOURCE_TEXEL_HEIGHT",m->TexelHeight);
  geo_typed_resource_trace(h,"RESOURCE_TEXEL_DEPTH",m->TexelDepth);
  geo_typed_resource_trace(h,"RESOURCE_PHYSICAL_WIDTH",m->PhysicalWidth);
  geo_typed_resource_trace(h,"RESOURCE_PHYSICAL_HEIGHT",m->PhysicalHeight);
  geo_typed_resource_trace(h,"RESOURCE_PHYSICAL_DEPTH",m->PhysicalDepth);
 }
}
static struct geo_typed_resource *resource_handle(D3D10DDI_HRESOURCE h) {
 return h.pDrvPrivate?((struct geo_typed_handle *)h.pDrvPrivate)->object:NULL;
}
struct geo_typed_resource *geo_typed_resource(D3D10DDI_HDEVICE h,D3D10DDI_HRESOURCE r) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_resource *p=resource_handle(r),*i;
 if(!d||!p)return NULL;
 for(i=d->resources;i&&i!=p;i=i->next){}
 return i&&i->magic==GEO_TYPED_RESOURCE_MAGIC&&!i->deleted?i:NULL;
}
void geo_typed_resource_ref(struct geo_typed_resource *r){if(r)r->refs++;}
HRESULT geo_typed_resource_unref(struct geo_typed_resource *r) {
 struct geo_typed_resource **link;HRESULT hr;struct geo_typed_device *d;
 if(!r||!r->refs||!(d=r->device))return E_INVALIDARG;
 if(--r->refs)return S_OK;
 hr=d->memory.pfnDestroyResource(&d->backend,r->backend);
 if(FAILED(hr)){r->refs=1;return hr;} /* ambiguous allocation stays pinned */
 for(link=&d->resources;*link&&*link!=r;link=&(*link)->next){}
 if(!*link)return E_UNEXPECTED;*link=r->next;r->magic=0;HeapFree(GetProcessHeap(),0,r);return S_OK;
}
static HRESULT desc_valid(const D3D11DDIARG_CREATERESOURCE *p) {
 const D3D10DDI_MIPINFO *m;
 if(!p||!p->pMipInfoList||p->MipLevels!=1||p->ArraySize!=1||p->MiscFlags||(!A04_GE8300_UNIFIED_DISPLAY356&&p->pPrimaryDesc)||p->ByteStride||p->DecoderBufferType)return E_NOTIMPL;
 m=p->pMipInfoList;
 if(p->pPrimaryDesc&&(!(p->BindFlags&D3D10_DDI_BIND_PRESENT)||p->pPrimaryDesc->VidPnSourceId||p->pPrimaryDesc->ModeDesc.Width!=m->TexelWidth||p->pPrimaryDesc->ModeDesc.Height!=m->TexelHeight))return E_NOTIMPL;
 if(!m->TexelWidth||!m->TexelHeight||m->TexelDepth!=1||m->PhysicalWidth!=m->TexelWidth||m->PhysicalHeight!=m->TexelHeight||m->PhysicalDepth!=1)return E_INVALIDARG;
 if(p->Usage>D3D10_DDI_USAGE_STAGING||(p->MapFlags&~D3D10_DDI_CPU_ACCESS_MASK))return E_INVALIDARG;
 if(p->ResourceDimension==D3D10DDIRESOURCE_BUFFER){
  if(m->TexelWidth>GEO_VERTEX_MAX_BYTES||m->TexelHeight!=1||p->Format!=DXGI_FORMAT_UNKNOWN||
     !(p->BindFlags&(D3D10_DDI_BIND_VERTEX_BUFFER|D3D10_DDI_BIND_INDEX_BUFFER|D3D10_DDI_BIND_CONSTANT_BUFFER))||
     (p->BindFlags&~(D3D10_DDI_BIND_VERTEX_BUFFER|D3D10_DDI_BIND_INDEX_BUFFER|D3D10_DDI_BIND_CONSTANT_BUFFER)))return E_NOTIMPL;
 } else if(p->ResourceDimension==D3D10DDIRESOURCE_TEXTURE2D){
  if(p->Format!=DXGI_FORMAT_B8G8R8A8_UNORM||m->TexelWidth>800u||m->TexelHeight>A04_NATIVE_FRAME_HEIGHT||
     p->SampleDesc.Count!=1||p->SampleDesc.Quality||(p->BindFlags&~(D3D10_DDI_BIND_RENDER_TARGET|D3D10_DDI_BIND_SHADER_RESOURCE|(A04_GE8300_UNIFIED_DISPLAY356?D3D10_DDI_BIND_PRESENT:0))))return E_NOTIMPL;
  if(p->BindFlags&D3D10_DDI_BIND_SHADER_RESOURCE){if(p->BindFlags!=D3D10_DDI_BIND_SHADER_RESOURCE||m->TexelWidth>128||m->TexelHeight>128)return E_NOTIMPL;}
 } else return E_NOTIMPL;
 if(p->Usage==D3D10_DDI_USAGE_STAGING&&(p->BindFlags||!p->MapFlags))return E_INVALIDARG;
 if(p->Usage==D3D10_DDI_USAGE_IMMUTABLE&&(!p->pInitialDataUP||p->MapFlags))return E_INVALIDARG;
 if(p->Usage==D3D10_DDI_USAGE_DEFAULT&&p->MapFlags)return E_INVALIDARG;
 if(p->Usage==D3D10_DDI_USAGE_DYNAMIC&&p->MapFlags!=D3D10_DDI_CPU_ACCESS_WRITE)return E_INVALIDARG;
 return S_OK;
}
static SIZE_T APIENTRY resource_size(D3D10DDI_HDEVICE h,const D3D11DDIARG_CREATERESOURCE *p) {
 HRESULT hr=desc_valid(p);if(!geo_typed_device(h))return 0;
 trace_descriptor(h,1,p);geo_typed_resource_trace(h,"RESOURCE_SIZE_VALIDATION",(UINT)hr);
 if(FAILED(hr)){geo_typed_error(h,hr);return 0;}return sizeof(struct geo_typed_handle);
}
static HRESULT map_backend(struct geo_typed_resource *r,D3D10_DDI_MAP mode,UINT flags,D3D10DDI_MAPPED_SUBRESOURCE *out) {
 D3DDDIARG_LOCK q;HRESULT hr;
 if(!out||r->mapped||(flags&~D3D10_DDI_MAP_FLAG_DONOTWAIT))return E_INVALIDARG;
 memset(out,0,sizeof(*out));memset(&q,0,sizeof(q));q.hResource=r->backend;
 q.Flags.ReadOnly=mode==D3D10_DDI_MAP_READ;q.Flags.DoNotWait=(flags&D3D10_DDI_MAP_FLAG_DONOTWAIT)!=0;
 hr=r->device->memory.pfnLock(&r->device->backend,&q);
 geo_typed_device_trace(r->device,"RESOURCE_LOCK_MODE",mode);
 geo_typed_device_trace(r->device,"RESOURCE_LOCK_HRESULT",(UINT)hr);
 if(SUCCEEDED(hr)){r->mapped=1;out->pData=q.pSurfData;out->RowPitch=q.Pitch;out->DepthPitch=q.SlicePitch;}
 return hr;
}
static HRESULT unmap_backend(struct geo_typed_resource *r) {
 D3DDDIARG_UNLOCK q;HRESULT hr;if(!r->mapped)return E_INVALIDARG;
 memset(&q,0,sizeof(q));q.hResource=r->backend;hr=r->device->memory.pfnUnlock(&r->device->backend,&q);geo_typed_device_trace(r->device,"RESOURCE_UNLOCK_HRESULT",(UINT)hr);if(SUCCEEDED(hr))r->mapped=0;return hr;
}
static void APIENTRY create_resource(D3D10DDI_HDEVICE h,const D3D11DDIARG_CREATERESOURCE *p,D3D10DDI_HRESOURCE handle,D3D10DDI_HRTRESOURCE runtime) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_resource *r;D3DDDIARG_CREATERESOURCE q;D3DDDI_SURFACEINFO surf;HRESULT hr;
 if(!d||!handle.pDrvPrivate){geo_typed_error(h,E_INVALIDARG);return;}
 ((struct geo_typed_handle *)handle.pDrvPrivate)->object=NULL;
 if(FAILED(d->error)){geo_typed_error(h,d->error);return;}
 trace_descriptor(h,2,p);
 hr=desc_valid(p);if(FAILED(hr)){geo_typed_error(h,hr);return;}
 r=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*r));if(!r){geo_typed_error(h,E_OUTOFMEMORY);return;}
 r->magic=GEO_TYPED_RESOURCE_MAGIC;r->refs=1;r->device=d;r->usage=p->Usage;r->bind=p->BindFlags;r->map_flags=p->MapFlags;r->format=p->Format;r->dimension=p->ResourceDimension;
 r->width=p->pMipInfoList[0].TexelWidth;r->height=p->pMipInfoList[0].TexelHeight;r->bytes=r->width*r->height*(r->dimension==D3D10DDIRESOURCE_BUFFER?1u:4u);
 memset(&q,0,sizeof(q));memset(&surf,0,sizeof(surf));surf.Width=r->width;surf.Height=r->height;surf.Depth=1;
 q.Format=r->dimension==D3D10DDIRESOURCE_BUFFER?D3DDDIFMT_VERTEXDATA:D3DDDIFMT_A8R8G8B8;
 q.Pool=(p->BindFlags&D3D10_DDI_BIND_RENDER_TARGET)?D3DDDIPOOL_VIDEOMEMORY:D3DDDIPOOL_SYSTEMMEM;
 q.hResource=runtime.handle;q.pSurfList=&surf;q.SurfCount=q.MipLevels=1;q.Rotation=D3DDDI_ROTATION_IDENTITY;
 q.Flags.Texture=(p->BindFlags&D3D10_DDI_BIND_SHADER_RESOURCE)!=0;
 /* RT8.1 explicitly supports NO_SCANOUT: request real Blt presentation
  * into the standard OS-owned primary rather than advertise fake flip. */
 if(p->pPrimaryDesc)p->pPrimaryDesc->DriverFlags|=DXGI_DDI_PRIMARY_DRIVER_FLAG_NO_SCANOUT;
 q.Flags.Primary=0;r->primary=0;
 q.Flags.VertexBuffer=r->dimension==D3D10DDIRESOURCE_BUFFER;q.Flags.RenderTarget=(p->BindFlags&D3D10_DDI_BIND_RENDER_TARGET)!=0;
 geo_typed_resource_trace(h,"RESOURCE_BACKEND_POOL",q.Pool);
 geo_typed_resource_trace(h,"RESOURCE_RUNTIME_HANDLE_PRESENT",runtime.handle!=NULL);
 hr=d->memory.pfnCreateResource(&d->backend,&q);
 geo_typed_resource_trace(h,"RESOURCE_BACKEND_CREATE_HRESULT",(UINT)hr);
 if(FAILED(hr)){HeapFree(GetProcessHeap(),0,r);geo_typed_error(h,hr);return;}
 r->backend=q.hResource;r->next=d->resources;d->resources=r;((struct geo_typed_handle *)handle.pDrvPrivate)->object=r;
 if(p->pInitialDataUP){
  D3D10DDI_MAPPED_SUBRESOURCE map;UINT y,row=r->dimension==D3D10DDIRESOURCE_BUFFER?r->width:r->width*4u;
  if(!p->pInitialDataUP->pSysMem||(r->height>1&&p->pInitialDataUP->SysMemPitch<row)){geo_typed_error(h,E_INVALIDARG);return;}
  hr=map_backend(r,D3D10_DDI_MAP_WRITE,0,&map);
  if(SUCCEEDED(hr)){
   for(y=0;y<r->height;y++)memcpy((BYTE *)map.pData+y*map.RowPitch,(const BYTE *)p->pInitialDataUP->pSysMem+y*p->pInitialDataUP->SysMemPitch,row);
   hr=unmap_backend(r);
  }
  if(FAILED(hr))geo_typed_error(h,hr);
 }
}
static void APIENTRY destroy_resource(D3D10DDI_HDEVICE h,D3D10DDI_HRESOURCE handle) {
 struct geo_typed_resource *r=geo_typed_resource(h,handle);HRESULT hr;
 if(!r){geo_typed_error(h,E_INVALIDARG);return;}if(r->mapped){geo_typed_error(h,E_FAIL);return;}
 r->deleted=1;((struct geo_typed_handle *)handle.pDrvPrivate)->object=NULL;hr=geo_typed_resource_unref(r);if(FAILED(hr))geo_typed_error(h,hr);
}
static void APIENTRY map_resource(D3D10DDI_HDEVICE h,D3D10DDI_HRESOURCE handle,UINT sub,D3D10_DDI_MAP mode,UINT flags,D3D10DDI_MAPPED_SUBRESOURCE *out) {
 struct geo_typed_resource *r=geo_typed_resource(h,handle);HRESULT hr=E_INVALIDARG;if(out)memset(out,0,sizeof(*out));
 if(r&&FAILED(r->device->error)){geo_typed_error(h,r->device->error);return;}
 if(r&&!sub&&out&&r->usage!=D3D10_DDI_USAGE_IMMUTABLE&&r->usage!=D3D10_DDI_USAGE_DEFAULT){
  UINT read=mode==D3D10_DDI_MAP_READ||mode==D3D10_DDI_MAP_READWRITE,write=mode!=D3D10_DDI_MAP_READ;
  if(mode>=D3D10_DDI_MAP_READ&&mode<=D3D10_DDI_MAP_WRITE_NOOVERWRITE&&(!read||(r->map_flags&D3D10_DDI_CPU_ACCESS_READ))&&(!write||(r->map_flags&D3D10_DDI_CPU_ACCESS_WRITE)))
   /* Current backend takes a real serialized Lock. It does not pretend
    * NOOVERWRITE/DISCARD renaming; waiting is conservative and preserves data. */
   hr=map_backend(r,mode,flags,out);
 }
 if(FAILED(hr))geo_typed_error(h,hr);
}
static void APIENTRY unmap_resource(D3D10DDI_HDEVICE h,D3D10DDI_HRESOURCE handle,UINT sub) {
 struct geo_typed_resource *r=geo_typed_resource(h,handle);HRESULT hr=r&&!sub?unmap_backend(r):E_INVALIDARG;if(FAILED(hr))geo_typed_error(h,hr);
}
HRESULT geo_typed_read_resource(struct geo_typed_resource *r,UINT offset,UINT bytes,void *data) {
 D3D10DDI_MAPPED_SUBRESOURCE map;HRESULT hr;
 if(!r||!data||offset>r->bytes||bytes>r->bytes-offset||r->dimension!=D3D10DDIRESOURCE_BUFFER)return E_INVALIDARG;
 hr=map_backend(r,D3D10_DDI_MAP_READ,0,&map);if(FAILED(hr))return hr;
 memcpy(data,(BYTE *)map.pData+offset,bytes);return unmap_backend(r);
}
static void APIENTRY copy_resource(D3D10DDI_HDEVICE h,D3D10DDI_HRESOURCE dst,D3D10DDI_HRESOURCE src) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_resource *a=geo_typed_resource(h,src),*b=geo_typed_resource(h,dst);D3DDDIARG_BLT q;HRESULT hr;
 if(!d||!a||!b||a==b||a->format!=b->format||a->width!=b->width||a->height!=b->height||b->dimension!=D3D10DDIRESOURCE_TEXTURE2D||b->usage!=D3D10_DDI_USAGE_STAGING){geo_typed_error(h,E_NOTIMPL);return;}
 if(FAILED(d->error)){geo_typed_error(h,d->error);return;}
 memset(&q,0,sizeof(q));q.hSrcResource=a->backend;q.hDstResource=b->backend;q.SrcRect.right=q.DstRect.right=a->width;q.SrcRect.bottom=q.DstRect.bottom=a->height;
 hr=d->memory.pfnBlt(&d->backend,&q);geo_typed_device_trace(d,"RESOURCE_COPY_HRESULT",(UINT)hr);if(FAILED(hr))geo_typed_error(h,hr);
}
static BOOL APIENTRY staging_busy(D3D10DDI_HDEVICE h,D3D10DDI_HRESOURCE handle) {
 struct geo_typed_resource *r=geo_typed_resource(h,handle);D3D10DDI_MAPPED_SUBRESOURCE map;HRESULT hr;
 if(!r){geo_typed_error(h,E_INVALIDARG);return TRUE;}
 hr=map_backend(r,D3D10_DDI_MAP_READ,D3D10_DDI_MAP_FLAG_DONOTWAIT,&map);
 if(hr==D3DDDIERR_WASSTILLDRAWING||hr==DXGI_ERROR_WAS_STILL_DRAWING)return TRUE;
 if(FAILED(hr)){geo_typed_error(h,hr);return TRUE;}
 hr=unmap_backend(r);if(FAILED(hr)){geo_typed_error(h,hr);return TRUE;}return FALSE;
}
static SIZE_T APIENTRY opened_size(D3D10DDI_HDEVICE h,const D3D10DDIARG_OPENRESOURCE *p){
 if(!A04_GE8300_UNIFIED_DISPLAY356||!geo_typed_device(h)||!p||p->NumAllocations!=1||!p->pOpenAllocationInfo||p->PrivateDriverDataSize)return 0;
 return sizeof(struct geo_typed_handle);
}
static void APIENTRY open_resource(D3D10DDI_HDEVICE h,const D3D10DDIARG_OPENRESOURCE *p,D3D10DDI_HRESOURCE handle,D3D10DDI_HRTRESOURCE runtime){
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_resource *r;struct geo_surface_desc desc;HRESULT hr;
 if(!d||!handle.pDrvPrivate||!opened_size(h,p)){geo_typed_error(h,E_INVALIDARG);return;}
 ((struct geo_typed_handle *)handle.pDrvPrivate)->object=NULL;
 r=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*r));if(!r){geo_typed_error(h,E_OUTOFMEMORY);return;}
 hr=geo_umd_open_primary(&d->backend,p->pOpenAllocationInfo,runtime.handle,&r->backend,&desc);
 if(FAILED(hr)){HeapFree(GetProcessHeap(),0,r);geo_typed_error(h,hr);return;}
 r->magic=GEO_TYPED_RESOURCE_MAGIC;r->refs=1;r->device=d;r->primary=1;r->usage=D3D10_DDI_USAGE_DEFAULT;
 r->bind=D3D10_DDI_BIND_PRESENT|(desc.usage==GEO_USAGE_RENDER_TARGET?D3D10_DDI_BIND_RENDER_TARGET:0);
 r->width=desc.width;r->height=desc.height;r->bytes=desc.pitch*desc.height;r->format=DXGI_FORMAT_B8G8R8A8_UNORM;r->dimension=D3D10DDIRESOURCE_TEXTURE2D;
 r->next=d->resources;d->resources=r;((struct geo_typed_handle *)handle.pDrvPrivate)->object=r;
}
void geo_typed_resources_install(D3D11_1DDI_DEVICEFUNCS *f) {
 if(A04_GE8300_UNIFIED_DISPLAY356){f->pfnCalcPrivateOpenedResourceSize=opened_size;f->pfnOpenResource=open_resource;}
 f->pfnCalcPrivateResourceSize=resource_size;f->pfnCreateResource=create_resource;f->pfnDestroyResource=destroy_resource;
 f->pfnResourceIsStagingBusy=staging_busy;f->pfnResourceMap=map_resource;f->pfnResourceUnmap=unmap_resource;f->pfnStagingResourceMap=map_resource;f->pfnStagingResourceUnmap=unmap_resource;
 f->pfnDynamicIABufferMapDiscard=map_resource;f->pfnDynamicIABufferMapNoOverwrite=map_resource;f->pfnDynamicIABufferUnmap=unmap_resource;
 f->pfnDynamicConstantBufferMapDiscard=map_resource;f->pfnDynamicConstantBufferUnmap=unmap_resource;f->pfnDynamicConstantBufferMapNoOverwrite=map_resource;
 f->pfnDynamicResourceMapDiscard=map_resource;f->pfnDynamicResourceUnmap=unmap_resource;f->pfnResourceCopy=copy_resource;
}

HRESULT geo_typed_wait_resource(struct geo_typed_resource *r) {
 D3D10DDI_MAPPED_SUBRESOURCE map;HRESULT hr=map_backend(r,D3D10_DDI_MAP_READ,0,&map);return FAILED(hr)?hr:unmap_backend(r);
}
HRESULT geo_typed_allocation(struct geo_typed_resource *r,D3DKMT_HANDLE *out) {
 struct geo_surface_desc desc;UINT usage;if(!r||!out)return E_INVALIDARG;
 usage=(r->bind&D3D10_DDI_BIND_RENDER_TARGET)?GEO_USAGE_RENDER_TARGET:(r->dimension==D3D10DDIRESOURCE_BUFFER?GEO_USAGE_VERTEX_BUFFER:((r->bind&D3D10_DDI_BIND_SHADER_RESOURCE)?GEO_USAGE_TEXTURE2D:GEO_USAGE_STAGING));
 return geo_umd_resource_info(&r->device->backend,r->backend,usage,&desc,out);
}
