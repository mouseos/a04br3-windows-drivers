/* SPDX-License-Identifier: BSD-2-Clause-Patent */
#include "bootstrap.h"
#include "../memory.h"
struct geo_umd_adapter {
 const struct geo_umd_frontend *frontend;
 void *frontend_context;
 struct geo_umd_device *devices;
};
static HRESULT APIENTRY get_caps(HANDLE handle,const D3DDDIARG_GETCAPS *p) {
 struct geo_umd_adapter *a=handle;
 if(!a||!p)return E_INVALIDARG;
 return a->frontend->get_caps(a->frontend_context,p);
}
static HRESULT APIENTRY close_adapter(HANDLE handle) {
 struct geo_umd_adapter *a=handle;
 if(!a)return E_INVALIDARG;
 if(a->devices)return E_FAIL;
 a->frontend->close(a->frontend_context);HeapFree(GetProcessHeap(),0,a);return S_OK;
}
static int required_frontend(const D3DDDI_DEVICEFUNCS *f) {
 /* This is a minimum consistency check, NOT a claim that every FL-required
  * operation is present. The real frontend must validate its complete caps
  * and mandatory-DDI matrix in open/initialize_device. */
 return f->pfnSetRenderState&&f->pfnValidateDevice&&f->pfnSetTexture&&f->pfnSetTextureStageState&&
  f->pfnCreateVertexShaderFunc&&f->pfnDeleteVertexShaderFunc&&f->pfnSetVertexShaderFunc&&
  f->pfnCreateVertexShaderDecl&&f->pfnDeleteVertexShaderDecl&&f->pfnSetVertexShaderDecl&&
  f->pfnCreatePixelShader&&f->pfnDeletePixelShader&&f->pfnSetPixelShader&&
  f->pfnSetVertexShaderConst&&f->pfnSetPixelShaderConst&&f->pfnSetStreamSource&&
  f->pfnSetIndices&&f->pfnDrawPrimitive&&f->pfnDrawIndexedPrimitive&&f->pfnFlush;
}
static HRESULT APIENTRY destroy_device(HANDLE handle) {
 struct geo_umd_device *d=handle,**link;struct geo_umd_adapter *a;D3DDDICB_DESTROYCONTEXT q;HRESULT hr;
 if(!d||!(a=d->adapter))return E_INVALIDARG;
 if(d->resources)return E_FAIL; /* Never hide a live runtime allocation. */
 hr=a->frontend->quiesce_device(d);if(FAILED(hr))return hr;
 memset(&q,0,sizeof(q));q.hContext=d->context;
 hr=d->cb.pfnDestroyContextCb(d->runtime,&q);if(FAILED(hr))return hr;
 a->frontend->destroy_device(d);
 for(link=&a->devices;*link&&*link!=d;link=&(*link)->next){}
 if(!*link)return E_UNEXPECTED;
 *link=d->next;HeapFree(GetProcessHeap(),0,d);return S_OK;
}
static HRESULT APIENTRY create_device(HANDLE handle,D3DDDIARG_CREATEDEVICE *p) {
 struct geo_umd_adapter *a=handle;struct geo_umd_device *d;
 D3DDDICB_CREATECONTEXT context;D3DDDICB_DESTROYCONTEXT rollback;D3DDDI_DEVICEFUNCS funcs;HRESULT hr,cleanup;
 if(!a||!p||!p->hDevice||!p->pDeviceFuncs||!p->pCallbacks||
    !p->pCallbacks->pfnCreateContextCb||!p->pCallbacks->pfnDestroyContextCb||p->Flags.Value)return E_INVALIDARG;
 d=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*d));if(!d)return E_OUTOFMEMORY;
 memset(&context,0,sizeof(context));context.NodeOrdinal=0;context.EngineAffinity=1;
 hr=p->pCallbacks->pfnCreateContextCb(p->hDevice,&context);
 if(FAILED(hr)){HeapFree(GetProcessHeap(),0,d);return hr;}
 hr=geo_umd_clear_init(d,p->hDevice,p->pCallbacks,&context);
 /* Keep actual callback/handle even if runtime returned malformed buffers;
  * rollback still uses the real created context, never a fabricated handle. */
 d->runtime=p->hDevice;d->context=context.hContext;d->cb=*p->pCallbacks;d->adapter=a;
 if(SUCCEEDED(hr)) {
  memset(&funcs,0,sizeof(funcs));hr=a->frontend->initialize_device(a->frontend_context,d,&funcs);
  if(SUCCEEDED(hr)){d->initialized=1;if(!required_frontend(&funcs))hr=E_NOTIMPL;}
  if(d->resources)hr=E_UNEXPECTED;
 }
 if(FAILED(hr)) {
  /* An ambiguous frontend partial allocation is pinned. initialize failure
   * must leave frontend_private NULL if it has already undone its effects. */
  if(d->resources||!context.hContext||(d->frontend_private&&!d->initialized)){d->next=a->devices;a->devices=d;return hr;}
  if(d->initialized) {
   cleanup=a->frontend->quiesce_device(d);
   if(FAILED(cleanup)){d->next=a->devices;a->devices=d;return hr;}
  }
  memset(&rollback,0,sizeof(rollback));rollback.hContext=context.hContext;
  cleanup=p->pCallbacks->pfnDestroyContextCb(p->hDevice,&rollback);
  if(FAILED(cleanup)){d->next=a->devices;a->devices=d;return hr;}
  if(d->initialized)a->frontend->destroy_device(d);
  HeapFree(GetProcessHeap(),0,d);return hr;
 }
 d->frontend_flush=funcs.pfnFlush;geo_umd_install_clear_ddis(&funcs);funcs.pfnDestroyDevice=destroy_device;
 d->next=a->devices;a->devices=d;
 *p->pDeviceFuncs=funcs;p->hDevice=d;
 p->pCommandBuffer=context.pCommandBuffer;p->CommandBufferSize=context.CommandBufferSize;
 p->pAllocationList=context.pAllocationList;p->AllocationListSize=context.AllocationListSize;
 p->pPatchLocationList=context.pPatchLocationList;p->PatchLocationListSize=context.PatchLocationListSize;
 p->CommandBuffer=context.CommandBuffer;return S_OK;
}
HRESULT GeoOpenAdapterWithFrontend(D3DDDIARG_OPENADAPTER *p,const struct geo_umd_frontend *f) {
 struct geo_umd_adapter *a;D3DDDI_ADAPTERFUNCS funcs;HRESULT hr;
 if(!p||!p->hAdapter||!p->pAdapterCallbacks||!p->pAdapterCallbacks->pfnQueryAdapterInfoCb||!p->pAdapterFuncs||
    !f||f->size!=sizeof(*f)||f->version!=1||!f->open||!f->get_caps||!f->initialize_device||
    !f->quiesce_device||!f->destroy_device||!f->close)return E_INVALIDARG;
 a=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*a));if(!a)return E_OUTOFMEMORY;
 a->frontend=f;hr=f->open(p,&a->frontend_context);
 if(FAILED(hr)) {
  /* open failure contract: no retained state if context==NULL. Otherwise
   * retain the ambiguous context; it is unsafe to invent a successful close. */
  if(!a->frontend_context)HeapFree(GetProcessHeap(),0,a);return hr;
 }
 if(!a->frontend_context)return E_UNEXPECTED;
 memset(&funcs,0,sizeof(funcs));funcs.pfnGetCaps=get_caps;funcs.pfnCreateDevice=create_device;funcs.pfnCloseAdapter=close_adapter;
 *p->pAdapterFuncs=funcs;p->DriverVersion=D3D_UMD_INTERFACE_VERSION;p->hAdapter=a;return S_OK;
}
