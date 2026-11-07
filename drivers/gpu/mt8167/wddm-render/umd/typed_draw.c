/* SPDX-License-Identifier: MIT */
#include "typed_device.h"
#include "../memory.h"
struct geo_typed_layout {struct geo_typed_layout *next;struct geo_typed_device *device;UINT refs,deleted,offset;};
static UINT bits(FLOAT f){UINT v;memcpy(&v,&f,4);return v;}
static void layout_unref(struct geo_typed_layout *v) {
 struct geo_typed_layout **p;if(!v||!v->refs||--v->refs)return;
 for(p=&v->device->layouts;*p&&*p!=v;p=&(*p)->next){}if(!*p)return;*p=v->next;HeapFree(GetProcessHeap(),0,v);
}
static struct geo_typed_layout *layout(D3D10DDI_HDEVICE h,D3D10DDI_HELEMENTLAYOUT handle) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_layout *v,*want=handle.pDrvPrivate?((struct geo_typed_handle *)handle.pDrvPrivate)->object:NULL;
 if(!d||!want)return NULL;for(v=d->layouts;v&&v!=want;v=v->next){}return v&&!v->deleted?v:NULL;
}
static HRESULT layout_desc(const D3D10DDIARG_CREATEELEMENTLAYOUT *p) {
 const D3D10DDIARG_INPUT_ELEMENT_DESC *e;if(!p||p->NumElements!=1||!p->pVertexElements)return E_NOTIMPL;e=p->pVertexElements;
 if(e->InputSlot||e->InputRegister||e->Format!=DXGI_FORMAT_R32G32B32A32_FLOAT||e->InputSlotClass!=D3D10_DDI_INPUT_PER_VERTEX_DATA||e->InstanceDataStepRate||e->AlignedByteOffset>65520u)return E_NOTIMPL;return S_OK;
}
static SIZE_T APIENTRY layout_size(D3D10DDI_HDEVICE h,const D3D10DDIARG_CREATEELEMENTLAYOUT *p) {HRESULT hr=layout_desc(p);if(FAILED(hr)){geo_typed_error(h,hr);return 0;}return sizeof(struct geo_typed_handle);}
static void APIENTRY create_layout(D3D10DDI_HDEVICE h,const D3D10DDIARG_CREATEELEMENTLAYOUT *p,D3D10DDI_HELEMENTLAYOUT handle,D3D10DDI_HRTELEMENTLAYOUT runtime) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_layout *v;HRESULT hr;(void)runtime;
 if(!d||!handle.pDrvPrivate){geo_typed_error(h,E_INVALIDARG);return;}((struct geo_typed_handle *)handle.pDrvPrivate)->object=NULL;
 hr=layout_desc(p);if(FAILED(hr)){geo_typed_error(h,hr);return;}
 v=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*v));if(!v){geo_typed_error(h,E_OUTOFMEMORY);return;}
 v->refs=1;v->device=d;v->offset=p->pVertexElements[0].AlignedByteOffset;v->next=d->layouts;d->layouts=v;((struct geo_typed_handle *)handle.pDrvPrivate)->object=v;
}
static void APIENTRY set_layout(D3D10DDI_HDEVICE h,D3D10DDI_HELEMENTLAYOUT handle) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_layout *v=NULL,*old;
 if(!d)return;if(handle.pDrvPrivate&&!(v=layout(h,handle))){geo_typed_error(h,E_INVALIDARG);return;}
 if(v)v->refs++;old=d->layout;d->layout=v;if(old)layout_unref(old);
}
static void APIENTRY destroy_layout(D3D10DDI_HDEVICE h,D3D10DDI_HELEMENTLAYOUT handle) {
 struct geo_typed_layout *v=layout(h,handle);if(!v){geo_typed_error(h,E_INVALIDARG);return;}v->deleted=1;((struct geo_typed_handle *)handle.pDrvPrivate)->object=NULL;layout_unref(v);
}
static void APIENTRY set_topology(D3D10DDI_HDEVICE h,D3D10_DDI_PRIMITIVE_TOPOLOGY topology) {
 struct geo_typed_device *d=geo_typed_device(h);if(!d)return;
 if(topology!=D3D10_DDI_PRIMITIVE_TOPOLOGY_UNDEFINED&&topology!=D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST&&topology!=D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP){geo_typed_error(h,E_NOTIMPL);return;}d->topology=topology;
}
static void APIENTRY set_vertices(D3D10DDI_HDEVICE h,UINT start,UINT count,const D3D10DDI_HRESOURCE *handles,const UINT *strides,const UINT *offsets) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_resource *v=NULL,*old;HRESULT hr;
 if(!d||start||count>1||(count&&(!handles||!strides||!offsets))){geo_typed_error(h,E_NOTIMPL);return;}
 if(count&&handles[0].pDrvPrivate){v=geo_typed_resource(h,handles[0]);if(!v||!(v->bind&D3D10_DDI_BIND_VERTEX_BUFFER)||strides[0]<16u||strides[0]>65536u||offsets[0]>v->bytes){geo_typed_error(h,E_INVALIDARG);return;}}
 if(v)geo_typed_resource_ref(v);old=d->vertex;d->vertex=v;d->vertex_stride=v?strides[0]:0;d->vertex_offset=v?offsets[0]:0;
 if(old){hr=geo_typed_resource_unref(old);if(FAILED(hr))geo_typed_error(h,hr);}
}
static void APIENTRY set_indices(D3D10DDI_HDEVICE h,D3D10DDI_HRESOURCE handle,DXGI_FORMAT format,UINT offset) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_resource *v=NULL,*old;UINT stride=format==DXGI_FORMAT_R16_UINT?2u:4u;HRESULT hr;
 if(!d)return;if(handle.pDrvPrivate){v=geo_typed_resource(h,handle);if(!v||!(v->bind&D3D10_DDI_BIND_INDEX_BUFFER)||(format!=DXGI_FORMAT_R16_UINT&&format!=DXGI_FORMAT_R32_UINT)||offset%stride||offset>v->bytes){geo_typed_error(h,E_INVALIDARG);return;}}
 if(v)geo_typed_resource_ref(v);old=d->index;d->index=v;d->index_offset=offset;d->index_stride=v?stride:0;
 if(old){hr=geo_typed_resource_unref(old);if(FAILED(hr))geo_typed_error(h,hr);}
}
static void APIENTRY set_viewports(D3D10DDI_HDEVICE h,UINT count,UINT clear,const D3D10_DDI_VIEWPORT *v) {
 struct geo_typed_device *d=geo_typed_device(h);UINT i;(void)clear;
 if(!d||count>1||(count&&!v)){geo_typed_error(h,E_NOTIMPL);return;}
 if(count){for(i=0;i<6;i++){UINT x;memcpy(&x,(const BYTE *)v+i*4,4);if((x&0x7f800000u)==0x7f800000u){geo_typed_error(h,E_INVALIDARG);return;}}
  if(v->Width<=0||v->Height<=0||v->MinDepth<0||v->MaxDepth>1||v->MinDepth>v->MaxDepth){geo_typed_error(h,E_INVALIDARG);return;}d->viewport=*v;
 }d->viewport_set=count;
}
static void APIENTRY set_scissors(D3D10DDI_HDEVICE h,UINT count,UINT clear,const D3D10_DDI_RECT *v) {
 struct geo_typed_device *d=geo_typed_device(h);(void)clear;
 if(!d||count>1||(count&&!v)){geo_typed_error(h,E_NOTIMPL);return;}
 if(count){if(v->left>v->right||v->top>v->bottom){geo_typed_error(h,E_INVALIDARG);return;}d->scissor=*v;}d->scissor_set=count;
}
static void span(BYTE *blob,UINT *at,struct geo_draw_span *s,const void *data,UINT bytes){s->offset=*at;s->bytes=bytes;memcpy(blob+*at,data,bytes);*at+=bytes;}
static void shader_packet(BYTE *blob,UINT *at,struct geo_draw_shader *s,const struct geo_umd_tokenized_shader *p,const UINT constants[128]) {
 s->format=GEO_SHADER_D3D10_TOKENS;s->stage=p->compiled.stage;s->temps=p->compiled.temps;s->shareds=p->compiled.shareds;s->vtxins=p->compiled.vtxins;s->vtxouts=p->compiled.vtxouts;s->input_mask=p->compiled.input_mask;s->constant_mask=p->compiled.constant_mask;
 span(blob,at,&s->tokens,p->tokens,p->token_bytes);span(blob,at,&s->usc,p->compiled.code,p->compiled.code_bytes);span(blob,at,&s->constants,constants,512);
}
HRESULT geo_typed_submit_draw(D3D10DDI_HDEVICE h,const FLOAT vertices[3][4]) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_draw_packet *p;struct geo_surface_desc target;D3DKMT_HANDLE ta,va;D3DDDICB_RENDER q;
 UINT constants[2][128],total=1024,at=0,stage,i;BYTE *blob;HRESULT hr;
 if(!d||!d->target||!d->bound_shader[0]||!d->bound_shader[1])return E_INVALIDARG;
 if(d->target->resource->width!=800||d->target->resource->height!=600)return E_NOTIMPL;
 hr=geo_typed_validate_native_state(h);if(FAILED(hr))return hr;
 if(!d->viewport_set||d->viewport.TopLeftX||d->viewport.TopLeftY||d->viewport.Width!=800.0f||d->viewport.Height!=600.0f||d->viewport.MinDepth||d->viewport.MaxDepth!=1.0f)return E_NOTIMPL;
 hr=geo_umd_target_info(&d->backend,&target,&ta);if(FAILED(hr))return hr;
 for(stage=0;stage<2;stage++){
  struct geo_umd_tokenized_shader *s=d->bound_shader[stage]->code;
  if(s->token_bytes>GEO_DRAW_MAX_TOKENS*4u||s->compiled.code_bytes>GEO_DRAW_MAX_CODE)return E_INVALIDARG;
  total+=s->token_bytes+s->compiled.code_bytes;hr=geo_typed_shader_constants(d,stage,constants[stage]);if(FAILED(hr))return hr;
 }
 for(i=0;i<12u;i++){UINT b;memcpy(&b,(const BYTE *)vertices+i*4,4);if((b&0x7f800000u)==0x7f800000u)return E_INVALIDARG;}
 if(total>GEO_DRAW_MAX_BLOB||d->backend.command_bytes<sizeof(*p)+total||d->backend.allocation_count<2)return E_OUTOFMEMORY;
 hr=geo_umd_write_private_vertex(&d->backend,&d->converted,vertices,48,&va);if(FAILED(hr))return hr;
 p=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*p)+total);if(!p)return E_OUTOFMEMORY;blob=(BYTE *)(p+1);
 p->magic=GEO_DRAW_MAGIC;p->size=sizeof(*p)+total;p->version=1;p->vertex_index=1;p->request.size=sizeof(p->request);p->request.version=1;p->request.blob_bytes=total;p->request.target_desc=target;
 p->request.state.topology=GEO_DRAW_TRIANGLE_LIST;p->request.state.vertex_count=3;p->request.state.vertex_stride=16;p->request.state.viewport[2]=bits(800.0f);p->request.state.viewport[3]=bits(600.0f);p->request.state.viewport[5]=bits(1.0f);p->request.state.color_write_mask=15;
 for(stage=0;stage<2;stage++)shader_packet(blob,&at,stage?&p->request.ps:&p->request.vs,d->bound_shader[stage]->code,constants[stage]);
 p->request.target=1;p->request.vertex=2;hr=geo_draw_validate_capture(&p->request,blob,total)==GEO_OK?S_OK:E_INVALIDARG;p->request.target=p->request.vertex=0;
 if(FAILED(hr)){HeapFree(GetProcessHeap(),0,p);return hr;}
 memcpy(d->backend.commands,p,p->size);memset(d->backend.allocations,0,2*sizeof(*d->backend.allocations));d->backend.allocations[0].hAllocation=ta;d->backend.allocations[0].WriteOperation=1;d->backend.allocations[1].hAllocation=va;
 memset(&q,0,sizeof(q));q.hContext=d->backend.context;q.CommandLength=p->size;q.NumAllocations=2;hr=d->backend.cb.pfnRenderCb(d->backend.runtime,&q);HeapFree(GetProcessHeap(),0,p);
 if(SUCCEEDED(hr)){d->pending=1;d->backend.commands=q.pNewCommandBuffer;d->backend.command_bytes=q.NewCommandBufferSize;d->backend.allocations=q.pNewAllocationList;d->backend.allocation_count=q.NewAllocationListSize;d->backend.patches=q.pNewPatchLocationList;d->backend.patch_count=q.NewPatchLocationListSize;}
 return hr;
}
static void draw(D3D10DDI_HDEVICE h,UINT count,UINT start,INT base,UINT indexed) {
 struct geo_typed_device *d=geo_typed_device(h);UINT triangles,j,k;FLOAT v[3][4];HRESULT hr;
 if(!d||!d->layout||!d->vertex||(indexed&&!d->index)){geo_typed_error(h,E_INVALIDARG);return;}
 if(!count)return;
 if(d->topology==D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST){if(count%3){geo_typed_error(h,E_INVALIDARG);return;}triangles=count/3;}
 else if(d->topology==D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP){if(count<3)return;triangles=count-2;}
 else {geo_typed_error(h,E_NOTIMPL);return;}
 if(triangles>65535u){geo_typed_error(h,E_NOTIMPL);return;}
 for(j=0;j<triangles;j++){
  UINT slot[3];slot[0]=d->topology==D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST?3*j:j;slot[1]=slot[0]+1;slot[2]=slot[0]+2;
  if(d->topology==D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP&&(j&1)){UINT x=slot[1];slot[1]=slot[2];slot[2]=x;}
  for(k=0;k<3;k++){
   uint64_t index=(uint64_t)start+slot[k],offset;int64_t vertex;
   if(indexed){UINT n=0;BYTE raw[4];offset=(uint64_t)d->index_offset+index*d->index_stride;if(offset>UINT32_MAX){geo_typed_error(h,E_INVALIDARG);return;}
    hr=geo_typed_read_resource(d->index,(UINT)offset,d->index_stride,raw);if(FAILED(hr)){geo_typed_error(h,hr);return;}
    if(d->index_stride==2){USHORT s;memcpy(&s,raw,2);n=s;}else memcpy(&n,raw,4);vertex=(int64_t)n+base;
   }else vertex=(int64_t)index;
   if(vertex<0||vertex>65534){geo_typed_error(h,E_INVALIDARG);return;}
   offset=(uint64_t)d->vertex_offset+(uint64_t)vertex*d->vertex_stride+d->layout->offset;
   if(offset>UINT32_MAX){geo_typed_error(h,E_INVALIDARG);return;}
   hr=geo_typed_read_resource(d->vertex,(UINT)offset,16,v[k]);if(FAILED(hr)){geo_typed_error(h,hr);return;}
  }
  hr=geo_typed_submit_draw(h,v);if(FAILED(hr)){geo_typed_error(h,hr);return;}
 }
}
static void APIENTRY draw_vertices(D3D10DDI_HDEVICE h,UINT count,UINT first){draw(h,count,first,0,0);}
static void APIENTRY draw_indices(D3D10DDI_HDEVICE h,UINT count,UINT first,INT base){draw(h,count,first,base,1);}
HRESULT geo_typed_draw_unbind(D3D10DDI_HDEVICE h) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_typed_resource *v,*i;struct geo_typed_layout *l;HRESULT hr;if(!d)return E_INVALIDARG;
 v=d->vertex;i=d->index;l=d->layout;d->vertex=d->index=NULL;d->layout=NULL;if(l)layout_unref(l);
 if(v){hr=geo_typed_resource_unref(v);if(FAILED(hr))return hr;}if(i){hr=geo_typed_resource_unref(i);if(FAILED(hr))return hr;}
 return geo_umd_destroy_private_vertex(&d->backend,&d->converted);
}
void geo_typed_draw_install(D3D11_1DDI_DEVICEFUNCS *f) {
 f->pfnCalcPrivateElementLayoutSize=layout_size;f->pfnCreateElementLayout=create_layout;f->pfnDestroyElementLayout=destroy_layout;f->pfnIaSetInputLayout=set_layout;
 f->pfnIaSetTopology=set_topology;f->pfnIaSetVertexBuffers=set_vertices;f->pfnIaSetIndexBuffer=set_indices;f->pfnSetViewports=set_viewports;f->pfnSetScissorRects=set_scissors;f->pfnDraw=draw_vertices;f->pfnDrawIndexed=draw_indices;
}
