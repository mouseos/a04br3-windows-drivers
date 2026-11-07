/* SPDX-License-Identifier: MIT */
#include "typed_device.h"
#include "../memory.h"
struct geo_typed_query {struct geo_typed_query *next;struct geo_typed_device *device;UINT ended;HRESULT result;};
static struct geo_typed_query *query(D3D10DDI_HDEVICE h,D3D10DDI_HQUERY q) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_query *p,*want=q.pDrvPrivate?((struct geo_typed_handle *)q.pDrvPrivate)->object:NULL;
 if(!d||!want)return NULL;for(p=d->queries;p&&p!=want;p=p->next){}return p;
}
static SIZE_T APIENTRY query_size(D3D10DDI_HDEVICE h,const D3D10DDIARG_CREATEQUERY *p) {
 if(!geo_typed_device(h)||!p||p->Query!=D3D10DDI_QUERY_EVENT||p->MiscFlags){geo_typed_error(h,E_NOTIMPL);return 0;}return sizeof(struct geo_typed_handle);
}
static void APIENTRY create_query(D3D10DDI_HDEVICE h,const D3D10DDIARG_CREATEQUERY *p,D3D10DDI_HQUERY handle,D3D10DDI_HRTQUERY runtime) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_query *q;(void)runtime;
 if(!d||!handle.pDrvPrivate){geo_typed_error(h,E_INVALIDARG);return;}((struct geo_typed_handle *)handle.pDrvPrivate)->object=NULL;
 if(!query_size(h,p))return;q=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*q));if(!q){geo_typed_error(h,E_OUTOFMEMORY);return;}
 q->device=d;q->next=d->queries;d->queries=q;((struct geo_typed_handle *)handle.pDrvPrivate)->object=q;
}
static void APIENTRY query_end(D3D10DDI_HDEVICE h,D3D10DDI_HQUERY handle) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_query *q=query(h,handle);struct geo_typed_resource *r;HRESULT hr=S_OK;
 if(!d||!q){geo_typed_error(h,E_INVALIDARG);return;}
 q->ended=0;
 /* Current native context submits only jobs that WRITE a retained RT. A
  * real blocking runtime Lock of every such allocation waits for its actual
  * scheduler fence. This is an inefficient but genuine EVENT boundary, not
  * a timer, private polling result or proof of hardware quiescence. */
 for(r=d->resources;r;r=r->next)if(r->bind&D3D10_DDI_BIND_RENDER_TARGET){hr=geo_typed_wait_resource(r);if(FAILED(hr))break;}
 q->result=hr;q->ended=1;if(FAILED(hr))geo_typed_error(h,hr);
}
static void APIENTRY query_begin(D3D10DDI_HDEVICE h,D3D10DDI_HQUERY q){(void)q;geo_typed_error(h,E_INVALIDARG); /* EVENT has End, never Begin. */}
static void APIENTRY query_data(D3D10DDI_HDEVICE h,D3D10DDI_HQUERY handle,void *out,UINT bytes,UINT flags) {
 struct geo_typed_query *q=query(h,handle);
 if((flags&~D3D10_DDI_GET_DATA_DO_NOT_FLUSH)||!q||(out&&bytes!=sizeof(BOOL))||(!out&&bytes)){geo_typed_error(h,E_INVALIDARG);return;}
 if(!q->ended){geo_typed_error(h,S_FALSE);return;}if(FAILED(q->result)){geo_typed_error(h,q->result);return;}
 if(out)*(BOOL *)out=TRUE; /* Only after the actual completed-fence Lock above. */
}
static void APIENTRY destroy_query(D3D10DDI_HDEVICE h,D3D10DDI_HQUERY handle) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_query *q=query(h,handle),**p;if(!d||!q){geo_typed_error(h,E_INVALIDARG);return;}
 for(p=&d->queries;*p&&*p!=q;p=&(*p)->next){}if(!*p){geo_typed_error(h,E_UNEXPECTED);return;}*p=q->next;((struct geo_typed_handle *)handle.pDrvPrivate)->object=NULL;HeapFree(GetProcessHeap(),0,q);
}
static void APIENTRY counters(D3D10DDI_HDEVICE h,D3D10DDI_COUNTER_INFO *p){if(!p){geo_typed_error(h,E_INVALIDARG);return;}memset(p,0,sizeof(*p));}
static void APIENTRY null_shader(D3D10DDI_HDEVICE h,D3D10DDI_HSHADER s){if(s.pDrvPrivate)geo_typed_error(h,E_NOTIMPL);}
static void APIENTRY null_resources(D3D10DDI_HDEVICE h,UINT first,UINT count,const D3D10DDI_HSHADERRESOURCEVIEW *s) {
 UINT i;if(first>128||count>128-first||(count&&!s)){geo_typed_error(h,E_INVALIDARG);return;}for(i=0;i<count;i++)if(s[i].pDrvPrivate){geo_typed_error(h,E_NOTIMPL);return;}
 /* Actual unbound state is already empty; non-null SRVs are not supported. */
}
static void APIENTRY null_constants(D3D10DDI_HDEVICE h,UINT first,UINT count,const D3D10DDI_HRESOURCE *s,const UINT *a,const UINT *b) {
 UINT i;(void)a;(void)b;if(first>14||count>14-first||(count&&!s)){geo_typed_error(h,E_INVALIDARG);return;}for(i=0;i<count;i++)if(s[i].pDrvPrivate){geo_typed_error(h,E_NOTIMPL);return;}
}
static void APIENTRY null_predication(D3D10DDI_HDEVICE h,D3D10DDI_HQUERY q,BOOL value){(void)value;if(q.pDrvPrivate)geo_typed_error(h,E_NOTIMPL);}
static void APIENTRY shader_vs_interfaces(D3D10DDI_HDEVICE h,D3D10DDI_HSHADER s,UINT count,const UINT *ids,const D3D11DDIARG_POINTERDATA *data){struct geo_typed_device *d=geo_typed_device(h);(void)ids;(void)data;if(!d)return;if(count){geo_typed_error(h,E_NOTIMPL);return;}d->functions.pfnVsSetShader(h,s);}
static void APIENTRY shader_ps_interfaces(D3D10DDI_HDEVICE h,D3D10DDI_HSHADER s,UINT count,const UINT *ids,const D3D11DDIARG_POINTERDATA *data){struct geo_typed_device *d=geo_typed_device(h);(void)ids;(void)data;if(!d)return;if(count){geo_typed_error(h,E_NOTIMPL);return;}d->functions.pfnPsSetShader(h,s);}
static void APIENTRY hazard(D3D10DDI_HDEVICE h,D3D10DDI_HRESOURCE handle){struct geo_typed_resource *r=geo_typed_resource(h,handle);HRESULT hr=r?geo_typed_wait_resource(r):E_INVALIDARG;if(FAILED(hr))geo_typed_error(h,hr);}
void geo_typed_runtime_install(D3D11_1DDI_DEVICEFUNCS *f) {
 f->pfnCalcPrivateQuerySize=query_size;f->pfnCreateQuery=create_query;f->pfnDestroyQuery=destroy_query;f->pfnQueryBegin=query_begin;f->pfnQueryEnd=query_end;f->pfnQueryGetData=query_data;f->pfnCheckCounterInfo=counters;
 f->pfnGsSetShader=null_shader;f->pfnHsSetShader=null_shader;f->pfnDsSetShader=null_shader;f->pfnCsSetShader=null_shader;
 f->pfnPsSetShaderResources=null_resources;f->pfnVsSetShaderResources=null_resources;f->pfnGsSetShaderResources=null_resources;f->pfnHsSetShaderResources=null_resources;f->pfnDsSetShaderResources=null_resources;f->pfnCsSetShaderResources=null_resources;
 f->pfnGsSetConstantBuffers=null_constants;f->pfnHsSetConstantBuffers=null_constants;f->pfnDsSetConstantBuffers=null_constants;f->pfnCsSetConstantBuffers=null_constants;
 f->pfnSetPredication=null_predication;f->pfnVsSetShaderWithIfaces=shader_vs_interfaces;f->pfnPsSetShaderWithIfaces=shader_ps_interfaces;f->pfnResourceReadAfterWriteHazard=hazard;
}
