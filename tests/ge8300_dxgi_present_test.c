/* SPDX-License-Identifier: MIT. Real DXGI dispatch, mocked OS allocation/runtime boundary. */
#include "../drivers/gpu/mt8167/wddm-render/umd/typed_device.h"
#include "../drivers/gpu/mt8167/wddm-render/memory.h"
#define CHECK(x) do {if(!(x))ExitProcess(__LINE__);}while(0)
static struct geo_typed_device dev;
static struct geo_typed_resource src,dst;
static DXGI1_2_DDI_BASE_FUNCTIONS table;
static DXGI_DDI_ARG_PRESENT args;
static UINT flushes,callbacks,translations,reenter;
static HRESULT flush_result,allocation_result,callback_result,release_result;
void *memset(void *p,int v,size_t n){BYTE *b=p;while(n--)*b++=(BYTE)v;return p;}
struct geo_typed_device *geo_typed_device(D3D10DDI_HDEVICE h){return h.pDrvPrivate==&dev?&dev:NULL;}
struct geo_typed_resource *geo_typed_resource(D3D10DDI_HDEVICE h,D3D10DDI_HRESOURCE r){
 if(h.pDrvPrivate!=&dev)return NULL;
 if(r.pDrvPrivate==&src&&!src.deleted)return &src;
 if(r.pDrvPrivate==&dst&&!dst.deleted)return &dst;
 return NULL;
}
void geo_typed_resource_ref(struct geo_typed_resource *r){r->refs++;}
HRESULT geo_typed_resource_unref(struct geo_typed_resource *r){CHECK(r->refs>1);r->refs--;return release_result;}
HRESULT geo_typed_allocation(struct geo_typed_resource *r,D3DKMT_HANDLE *out){translations++;*out=r==&src?0x123:0x456;return allocation_result;}
static HRESULT APIENTRY flush(HANDLE h){CHECK(h==&dev.backend);flushes++;return flush_result;}
static HRESULT APIENTRY callback(HANDLE h,const DXGIDDICB_PRESENT *q){
 UINT i;callbacks++;CHECK(flushes==1&&translations==(args.hDstResource?2u:1u));
 CHECK(h==dev.backend.runtime&&q->hContext==dev.backend.context&&q->pDXGIContext==args.pDXGIContext);
 CHECK(q->hSrcAllocation==0x123&&q->hDstAllocation==(args.hDstResource?0x456u:0u));
 CHECK(!q->BroadcastContextCount&&src.refs==2&&(!args.hDstResource||dst.refs==2));
 for(i=0;i<D3DDDI_MAX_BROADCAST_CONTEXT;i++)CHECK(!q->BroadcastContext[i]);
 if(reenter)CHECK(table.pfnPresent(&args)==DXGI_ERROR_WAS_STILL_DRAWING);
 return callback_result;
}
static void reset(void){
 memset(&dev,0,sizeof(dev));memset(&src,0,sizeof(src));memset(&dst,0,sizeof(dst));memset(&args,0,sizeof(args));
 dev.present_callback=callback;dev.backend.runtime=(HANDLE)(UINT_PTR)0x7000;dev.backend.context=(HANDLE)(UINT_PTR)0x8000;dev.backend.frontend_flush=flush;
 src.refs=dst.refs=1;src.device=dst.device=&dev;src.width=dst.width=800;src.height=dst.height=600;
 src.dimension=dst.dimension=D3D10DDIRESOURCE_TEXTURE2D;src.format=dst.format=DXGI_FORMAT_B8G8R8A8_UNORM;src.bind=D3D10_DDI_BIND_RENDER_TARGET;
 args.hDevice=(DXGI_DDI_HDEVICE)&dev;args.hSurfaceToPresent=(DXGI_DDI_HRESOURCE)&src;args.hDstResource=(DXGI_DDI_HRESOURCE)&dst;
 args.pDXGIContext=(void *)(UINT_PTR)0xdead;args.Flags.Value=1;
 flushes=callbacks=translations=reenter=0;flush_result=allocation_result=callback_result=release_result=S_OK;
}
void mainCRTStartup(void){
 geo_typed_dxgi_install(&table);
 reset();reenter=1;CHECK(table.pfnPresent(&args)==S_OK&&callbacks==1&&src.refs==1&&dst.refs==1&&!dev.presenting);
 reset();args.hDstResource=0;CHECK(table.pfnPresent(&args)==S_OK&&callbacks==1&&translations==1);
 reset();callback_result=DXGI_ERROR_UNSUPPORTED;CHECK(table.pfnPresent(&args)==callback_result&&callbacks==1&&src.refs==1);
 reset();callback_result=DXGI_ERROR_DEVICE_REMOVED;release_result=E_FAIL;CHECK(table.pfnPresent(&args)==callback_result);
 reset();release_result=E_FAIL;CHECK(table.pfnPresent(&args)==E_FAIL&&!dev.presenting);
 reset();flush_result=DXGI_ERROR_DEVICE_HUNG;CHECK(table.pfnPresent(&args)==flush_result&&!callbacks&&!translations&&src.refs==1);
 reset();allocation_result=E_FAIL;CHECK(table.pfnPresent(&args)==E_FAIL&&!callbacks&&translations==1&&src.refs==1&&dst.refs==1);
 reset();dev.error=DXGI_ERROR_DEVICE_REMOVED;CHECK(table.pfnPresent(&args)==dev.error&&!flushes);
 reset();dev.present_callback=NULL;CHECK(table.pfnPresent(&args)==DXGI_ERROR_UNSUPPORTED&&!flushes);
 reset();args.Flags.Value=2;CHECK(table.pfnPresent(&args)==DXGI_ERROR_UNSUPPORTED&&!flushes);
 reset();args.SrcSubResourceIndex=1;CHECK(table.pfnPresent(&args)==DXGI_ERROR_UNSUPPORTED&&!flushes);
 reset();args.FlipInterval=DXGI_DDI_FLIP_INTERVAL_ONE;CHECK(table.pfnPresent(&args)==DXGI_ERROR_UNSUPPORTED&&!flushes);
 reset();src.mapped=1;CHECK(table.pfnPresent(&args)==E_INVALIDARG&&!callbacks&&!flushes);
 reset();dst.deleted=1;CHECK(table.pfnPresent(&args)==E_INVALIDARG&&!callbacks);
 reset();dst.width=1;CHECK(table.pfnPresent(&args)==E_INVALIDARG&&!callbacks);
 reset();args.hDstResource=args.hSurfaceToPresent;CHECK(table.pfnPresent(&args)==E_INVALIDARG&&!callbacks);
 CHECK(table.pfnPresent(NULL)==E_INVALIDARG);
 ExitProcess(0);
}
