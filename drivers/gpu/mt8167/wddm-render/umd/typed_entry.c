/* SPDX-License-Identifier: MIT */
#include "typed_device.h"
#include "typed_state.h"
#include "../memory.h"
/* Every advertised feature level is a complete runtime contract. These are
 * implementation obligations, not registry options or opt-in fake caps. */
#define GEO_NEED_SM4_COMPLETE 1u
#define GEO_NEED_TEXTURE_2048 2u
#define GEO_NEED_DEPTH_STENCIL 4u
#define GEO_NEED_BLEND 8u
#define GEO_NEED_NATIVE_STATE 16u
#define GEO_NEED_REQUIRED_DDI 32u
#define GEO_NEED_KMD_RECOVERY 64u
#if defined(GEO_TYPED_NATIVE_PIPELINE10) && !defined(GEO_TYPED_LIMITED_DEVELOPMENT)
#error Native pipeline bring-up requires the explicit limited development build
#endif
/* RT8.1 routes FL9 requests through D3D10Level9's legacy OpenAdapter.
 * This optional development configuration exercises our actual typed DDI
 * through the native FL10 path. It is not full feature-level qualification;
 * unsupported operations still fail through the real runtime ErrorCb. */
static D3D11DDI_3DPIPELINELEVEL development_pipeline(void) {
#ifdef GEO_TYPED_NATIVE_PIPELINE10
 return D3D11DDI_3DPIPELINELEVEL_10_0;
#else
 return D3D11_1DDI_3DPIPELINELEVEL_9_1;
#endif
}
UINT geo_typed_profile_missing(void) {
 /* See TYPED-COMPATIBILITY.md. Remove a bit only with the real corresponding
  * implementation and native contract, never because the smoke client omits it. */
 return GEO_NEED_SM4_COMPLETE|GEO_NEED_TEXTURE_2048|GEO_NEED_DEPTH_STENCIL|GEO_NEED_BLEND|
        GEO_NEED_NATIVE_STATE|GEO_NEED_REQUIRED_DDI|GEO_NEED_KMD_RECOVERY;
}
static UINT development_enabled(void) {
#ifdef GEO_TYPED_LIMITED_DEVELOPMENT
 return 1;
#else
 return 0;
#endif
}
static void trace_value(struct geo_typed_adapter *a,const char *key,UINT value) {
#ifdef GEO_TYPED_LIMITED_DEVELOPMENT
 char line[120];const char hex[]="0123456789abcdef";UINT at=0,i;DWORD written;
 if(!a||a->trace==INVALID_HANDLE_VALUE||!a->trace)return;
 while(*key&&at<100)line[at++]=*key++;line[at++]='=';
 for(i=0;i<8;i++)line[at++]=hex[(value>>(28-4*i))&15];line[at++]='\r';line[at++]='\n';
 if(WriteFile(a->trace,line,at,&written,NULL)&&written==at)FlushFileBuffers(a->trace);
#else
 (void)a;(void)key;(void)value;
#endif
}
static HANDLE trace_open(void) {
#ifdef GEO_TYPED_LIMITED_DEVELOPMENT
 WCHAR path[96];const WCHAR prefix[]=L"C:\\MT8167D3DClear\\umd-",hex[]=L"0123456789abcdef";
 static volatile LONG serial;UINT i,n=0,value,attempt;HANDLE file;
 CreateDirectoryW(L"C:\\MT8167D3DClear",NULL);
 for(attempt=0;attempt<16;attempt++){
  n=0;for(i=0;prefix[i];i++)path[n++]=prefix[i];value=GetCurrentProcessId();for(i=0;i<8;i++)path[n++]=hex[(value>>(28-4*i))&15];path[n++]='-';value=GetTickCount()^(UINT)InterlockedIncrement(&serial);
  for(i=0;i<8;i++)path[n++]=hex[(value>>(28-4*i))&15];path[n++]='.';path[n++]='l';path[n++]='o';path[n++]='g';path[n]=0;
  file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,NULL);
  if(file!=INVALID_HANDLE_VALUE||GetLastError()!=ERROR_FILE_EXISTS)return file;
 }
#endif
 return INVALID_HANDLE_VALUE;
}
struct geo_typed_device *geo_typed_device(D3D10DDI_HDEVICE h) {
 struct geo_typed_device *d=h.pDrvPrivate?((struct geo_typed_handle *)h.pDrvPrivate)->object:NULL;
 return d&&d->magic==GEO_TYPED_DEVICE_MAGIC?d:NULL;
}
struct geo_typed_state *geo_typed_state_from_device(D3D10DDI_HDEVICE h){struct geo_typed_device *d=geo_typed_device(h);return d?d->state:NULL;}
void geo_typed_error(D3D10DDI_HDEVICE h,HRESULT hr) {
 struct geo_typed_device *d=geo_typed_device(h);if(!d||hr==S_OK)return;
 trace_value(d->adapter,"DDI_ERROR_HRESULT",(UINT)hr);
 if(FAILED(hr)&&SUCCEEDED(d->error))d->error=hr;
 if(d->callbacks.pfnSetErrorCb)d->callbacks.pfnSetErrorCb(d->core,hr);
}
HRESULT geo_typed_validate_native_state(D3D10DDI_HDEVICE h) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_state_snapshot *s;UINT used[GEO_TYPED_STATE_STAGES]={0},unsupported;HRESULT hr;
 if(!d||!d->state)return E_INVALIDARG;
 /* Current compiler rejects sampler instructions; zero comes from that real
  * compiler contract, not from ignoring currently bound SRV/sampler state. */
 s=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*s));if(!s)return E_OUTOFMEMORY;
 s->size=sizeof(*s);s->abi=GEO_TYPED_STATE_ABI;hr=geo_typed_state_snapshot(d->state,s);
 if(SUCCEEDED(hr)&&s->raster.ScissorEnable){
  if(!d->scissor_set||d->scissor.left||d->scissor.top||d->scissor.right!=800||d->scissor.bottom!=600)hr=E_NOTIMPL;
  /* Only the exact full extent of the existing 800x600 draw profile has no
   * clipping effect. Normalize the temporary snapshot, never the bound state.
   * All other raster/depth/blend/texture restrictions remain in flat preflight. */
  else s->raster.ScissorEnable=FALSE;
 }
 if(SUCCEEDED(hr))hr=geo_typed_state_validate_flat(s,used,FALSE,&unsupported);
 HeapFree(GetProcessHeap(),0,s);return hr;
}
static BOOL APIENTRY flush(D3D10DDI_HDEVICE h,UINT flags) {
 struct geo_typed_device *d=geo_typed_device(h);(void)flags;
 if(!d)return FALSE;if(FAILED(d->error))return FALSE;
 /* Clear/Draw submit immediately via real RenderCb; no deferred commands
  * require this process to run after return. TRUE is flushed, never idle or
  * GPU completion. Unknown flags conservatively flush per WDK contract. */
 d->pending=0;return TRUE;
}
static void APIENTRY relocate(D3D10DDI_HDEVICE h,D3D11_1DDI_DEVICEFUNCS *table){struct geo_typed_device *d=geo_typed_device(h);if(!d||!table){geo_typed_error(h,E_INVALIDARG);return;}*table=d->functions;}
static void APIENTRY check_format(D3D10DDI_HDEVICE h,DXGI_FORMAT format,UINT *support) {
 if(!support){geo_typed_error(h,E_INVALIDARG);return;}*support=0;
 if(format==DXGI_FORMAT_R32G32B32A32_FLOAT)*support=D3D11_1DDI_FORMAT_SUPPORT_VERTEX_BUFFER;
 if(format==DXGI_FORMAT_B8G8R8A8_UNORM)*support=D3D10_DDI_FORMAT_SUPPORT_RENDERTARGET;
 /* No texture/filter/blend/depth support follows merely from an allocation. */
}
static void APIENTRY check_samples(D3D10DDI_HDEVICE h,DXGI_FORMAT format,UINT samples,UINT *levels) {
 if(!levels){geo_typed_error(h,E_INVALIDARG);return;}*levels=(format==DXGI_FORMAT_B8G8R8A8_UNORM&&samples==1)?1u:0u;
}
static void APIENTRY destroy_device(D3D10DDI_HDEVICE h) {
 struct geo_typed_device *d=geo_typed_device(h);D3DDDICB_DESTROYCONTEXT q;HRESULT hr;
 if(!d)return;
 hr=geo_typed_views_unbind(h);if(SUCCEEDED(hr))hr=geo_typed_shaders_unbind(h);if(SUCCEEDED(hr))hr=geo_typed_draw_unbind(h);
 if(FAILED(hr)||d->resources||d->views||d->shaders||d->layouts||d->queries){geo_typed_error(h,FAILED(hr)?hr:E_FAIL);return;}
 memset(&q,0,sizeof(q));q.hContext=d->backend.context;hr=d->backend.cb.pfnDestroyContextCb(d->backend.runtime,&q);
 if(FAILED(hr)){geo_typed_error(h,hr);return;}
 geo_typed_state_delete(d->state);geo_pco_device_destroy(&d->compiler,d->compiler_device);d->adapter->devices--;d->magic=0;
 ((struct geo_typed_handle *)h.pDrvPrivate)->object=NULL;HeapFree(GetProcessHeap(),0,d);
}
static HRESULT APIENTRY backend_flush(HANDLE handle) {
 struct geo_umd_device *b=handle;struct geo_typed_device *d=b?b->adapter:NULL;
 return d?d->error:E_INVALIDARG; /* all actual commands are already submitted */
}
static SIZE_T APIENTRY device_size(D3D10DDI_HADAPTER h,const D3D10DDIARG_CALCPRIVATEDEVICESIZE *p) {
 if(!h.pDrvPrivate||!p||p->Interface!=D3D11_1_DDI_INTERFACE_VERSION)return 0;
 return sizeof(struct geo_typed_handle);
}
static HRESULT APIENTRY create_device(D3D10DDI_HADAPTER h,D3D10DDIARG_CREATEDEVICE *p) {
 struct geo_typed_adapter *a=h.pDrvPrivate;struct geo_typed_device *d;D3DDDICB_CREATECONTEXT q;D3DDDICB_DESTROYCONTEXT rollback;HRESULT hr,cleanup;
 if(!a||!p||!p->hDrvDevice.pDrvPrivate||p->Interface!=D3D11_1_DDI_INTERFACE_VERSION||!p->p11_1DeviceFuncs||!p->pKTCallbacks||!p->p11UMCallbacks||!p->p11UMCallbacks->pfnSetErrorCb||
    !p->pKTCallbacks->pfnCreateContextCb||!p->pKTCallbacks->pfnDestroyContextCb)return E_INVALIDARG;
 ((struct geo_typed_handle *)p->hDrvDevice.pDrvPrivate)->object=NULL;
 trace_value(a,"CREATE_DEVICE_FLAGS",p->Flags);trace_value(a,"CREATE_DEVICE_INTERFACE",p->Interface);
 if(D3D11DDI_EXTRACT_3DPIPELINELEVEL_FROM_FLAGS(p->Flags)!=development_pipeline()||(!development_enabled()&&geo_typed_profile_missing()))return DXGI_ERROR_UNSUPPORTED;
 d=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*d));if(!d)return E_OUTOFMEMORY;
 d->magic=GEO_TYPED_DEVICE_MAGIC;d->adapter=a;d->core=p->hRTCoreLayer;d->callbacks=*p->p11UMCallbacks;
 ((struct geo_typed_handle *)p->hDrvDevice.pDrvPrivate)->object=d;
 memset(&q,0,sizeof(q));q.NodeOrdinal=0;q.EngineAffinity=1;hr=p->pKTCallbacks->pfnCreateContextCb(p->hRTDevice.handle,&q);trace_value(a,"CREATE_CONTEXT_HRESULT",(UINT)hr);
 if(FAILED(hr))goto failed;
 hr=geo_umd_clear_init(&d->backend,p->hRTDevice.handle,p->pKTCallbacks,&q);trace_value(a,"MEMORY_INIT_HRESULT",(UINT)hr);
 if(SUCCEEDED(hr)){geo_umd_install_clear_ddis(&d->memory);d->backend.adapter=d;d->backend.frontend_flush=backend_flush;}
 if(SUCCEEDED(hr))hr=geo_pco_device_create(a->info.compiler_bvnc,&d->compiler,&d->compiler_device)==GEO_OK?S_OK:E_FAIL;
 trace_value(a,"PCO_INIT_HRESULT",(UINT)hr);
 if(SUCCEEDED(hr)){hr=geo_typed_state_create(&d->state);trace_value(a,"STATE_INIT_HRESULT",(UINT)hr);}
 if(FAILED(hr)){
  memset(&rollback,0,sizeof(rollback));rollback.hContext=q.hContext;cleanup=p->pKTCallbacks->pfnDestroyContextCb(p->hRTDevice.handle,&rollback);
  if(FAILED(cleanup)){a->devices++;return hr;} /* retain actual ambiguous context */
  goto failed;
 }
 geo_typed_resources_install(&d->functions);geo_typed_views_install(&d->functions);geo_typed_shaders_install(&d->functions);geo_typed_draw_install(&d->functions);geo_typed_state_install(&d->functions);
 geo_typed_runtime_install(&d->functions);geo_typed_dxgi_install(p->DXGIBaseDDI.pDXGIDDIBaseFunctions3);
 d->functions.pfnFlush=flush;d->functions.pfnRelocateDeviceFuncs=relocate;d->functions.pfnDestroyDevice=destroy_device;d->functions.pfnCheckFormatSupport=check_format;d->functions.pfnCheckMultisampleQualityLevels=check_samples;
 *p->p11_1DeviceFuncs=d->functions;a->devices++;trace_value(a,"DEVICE_READY",1);return S_OK;
 failed:
 if(d->state)geo_typed_state_delete(d->state);if(d->compiler_device)geo_pco_device_destroy(&d->compiler,d->compiler_device);
 ((struct geo_typed_handle *)p->hDrvDevice.pDrvPrivate)->object=NULL;d->magic=0;HeapFree(GetProcessHeap(),0,d);return hr;
}
static HRESULT APIENTRY close_adapter(D3D10DDI_HADAPTER h){struct geo_typed_adapter *a=h.pDrvPrivate;if(!a||a->devices)return E_FAIL;trace_value(a,"ADAPTER_CLOSE",0);if(a->trace&&a->trace!=INVALID_HANDLE_VALUE)CloseHandle(a->trace);HeapFree(GetProcessHeap(),0,a);return S_OK;}
static HRESULT APIENTRY versions(D3D10DDI_HADAPTER h,UINT32 *count,UINT64 *out) {
 UINT required=development_enabled()||!geo_typed_profile_missing()?1u:0u;
 if(!h.pDrvPrivate||!count)return E_INVALIDARG;
 if(out&&*count<required){*count=required;return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);}
 if(out&&required)out[0]=D3D11_1_DDI_SUPPORTED;*count=required;trace_value(h.pDrvPrivate,"SUPPORTED_VERSIONS",required);return S_OK;
}
static HRESULT APIENTRY caps_impl(D3D10DDI_HADAPTER h,const D3D10_2DDIARG_GETCAPS *p) {
 if(!h.pDrvPrivate||!p||!p->pData)return E_INVALIDARG;
 if(p->Type==D3D11_1DDICAPS_D3D11_OPTIONS){if(p->DataSize!=sizeof(D3D11_1DDI_D3D11_OPTIONS_DATA))return E_INVALIDARG;memset(p->pData,0,p->DataSize);return S_OK;}
 if(p->Type==D3D11_1DDICAPS_ARCHITECTURE_INFO){if(p->DataSize!=sizeof(D3D11_1DDI_ARCHITECTURE_INFO_DATA))return E_INVALIDARG;((D3D11_1DDI_ARCHITECTURE_INFO_DATA *)p->pData)->TileBasedDeferredRenderer=TRUE;return S_OK;}
 if(p->Type==D3D11_1DDICAPS_SHADER_MIN_PRECISION_SUPPORT){if(p->DataSize!=sizeof(D3D11_DDI_SHADER_MIN_PRECISION_SUPPORT_DATA))return E_INVALIDARG;memset(p->pData,0,p->DataSize);return S_OK;}
 if(p->DataSize!=sizeof(UINT))return E_INVALIDARG;
 switch(p->Type){
 case D3D11DDICAPS_3DPIPELINESUPPORT:*(UINT *)p->pData=(!development_enabled()&&geo_typed_profile_missing())?0:D3D11DDI_ENCODE_3DPIPELINESUPPORT_CAP(development_pipeline());return S_OK;
 case D3D11DDICAPS_THREADING:case D3D11DDICAPS_SHADER:*(UINT *)p->pData=0;return S_OK;
 default:return E_NOTIMPL;
 }
}
static HRESULT APIENTRY caps(D3D10DDI_HADAPTER h,const D3D10_2DDIARG_GETCAPS *p) {
 HRESULT hr=caps_impl(h,p);trace_value(h.pDrvPrivate,"CAP_TYPE",p?(UINT)p->Type:0xffffffffu);trace_value(h.pDrvPrivate,"CAP_HRESULT",(UINT)hr);return hr;
}
__declspec(dllexport) HRESULT APIENTRY OpenAdapter10_2(D3D10DDIARG_OPENADAPTER *p) {
 struct geo_typed_adapter *a;D3DDDICB_QUERYADAPTERINFO q;D3D10_2DDI_ADAPTERFUNCS table;HRESULT hr;
 if(!p||!p->pAdapterCallbacks||!p->pAdapterCallbacks->pfnQueryAdapterInfoCb||!p->pAdapterFuncs_2)return E_INVALIDARG;
 a=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*a));if(!a)return E_OUTOFMEMORY;a->runtime=p->hRTAdapter;a->trace=trace_open();trace_value(a,"OPEN_INTERFACE",p->Interface);trace_value(a,"OPEN_VERSION",p->Version);trace_value(a,"DEVELOPMENT_PROFILE",development_enabled());
 memset(&q,0,sizeof(q));q.pPrivateDriverData=&a->info;q.PrivateDriverDataSize=sizeof(a->info);hr=p->pAdapterCallbacks->pfnQueryAdapterInfoCb(p->hRTAdapter.handle,&q);trace_value(a,"QUERY_ADAPTER_HRESULT",(UINT)hr);
 if(FAILED(hr)){if(a->trace&&a->trace!=INVALID_HANDLE_VALUE)CloseHandle(a->trace);HeapFree(GetProcessHeap(),0,a);return hr;}
 if(a->info.size!=sizeof(a->info)||a->info.version!=2||a->info.magic!=GEO_ADAPTER_INFO_MAGIC||a->info.compiler_bvnc!=GEO_COMPILER_BVNC||a->info.draw_version!=1){trace_value(a,"ADAPTER_CONTRACT_FAILURE",1);if(a->trace&&a->trace!=INVALID_HANDLE_VALUE)CloseHandle(a->trace);HeapFree(GetProcessHeap(),0,a);return E_UNEXPECTED;}
 memset(&table,0,sizeof(table));table.pfnCalcPrivateDeviceSize=device_size;table.pfnCreateDevice=create_device;table.pfnCloseAdapter=close_adapter;table.pfnGetSupportedVersions=versions;table.pfnGetCaps=caps;
 *p->pAdapterFuncs_2=table;p->hAdapter.pDrvPrivate=a;return S_OK;
}
