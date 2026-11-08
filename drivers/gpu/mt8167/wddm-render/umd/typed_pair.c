/* SPDX-License-Identifier: MIT */
#include "typed_device.h"
void geo_typed_device_trace(struct geo_typed_device *,const char *,UINT);
#include "../compiler/pair_compiler.h"
#include "../memory.h"
static void pair_shader(BYTE *blob,UINT *at,struct geo_draw_shader *dst,const struct geo_umd_tokenized_shader *tokens,const struct geo_compiled_shader *code) {
 dst->format=GEO_SHADER_D3D10_TOKENS;dst->stage=code->stage;dst->temps=code->temps;dst->shareds=code->shareds;
 dst->vtxins=code->vtxins;dst->vtxouts=code->vtxouts;dst->input_mask=code->input_mask;dst->constant_mask=code->constant_mask;
 dst->tokens.offset=*at;dst->tokens.bytes=tokens->token_bytes;memcpy(blob+*at,tokens->tokens,tokens->token_bytes);*at+=tokens->token_bytes;
 dst->usc.offset=*at;dst->usc.bytes=code->code_bytes;memcpy(blob+*at,code->code,code->code_bytes);*at+=code->code_bytes;
 dst->constants.offset=*at;dst->constants.bytes=512;memset(blob+*at,0,512);*at+=512;
}
HRESULT geo_typed_submit_pair(D3D10DDI_HDEVICE h,const FLOAT vertices[3][8]) {
 struct geo_typed_device *d=geo_typed_device(h);struct geo_compiled_pair pair={0};struct geo_draw_packet_v2 *p=NULL;
 struct geo_umd_tokenized_shader *vs,*ps;struct geo_surface_desc target;D3DKMT_HANDLE ta,va;D3DDDICB_RENDER q;
 UINT total,at=0,i;BYTE *blob;int32_t status;HRESULT hr;
 if(!d||!d->target||!d->bound_shader[0]||!d->bound_shader[1])return E_INVALIDARG;
 if(d&&FAILED(d->error))return d->error;
 vs=d->bound_shader[0]->code;ps=d->bound_shader[1]->code;
 if(vs->pair_stage!=1||ps->pair_stage!=2||d->target->resource->width!=800||d->target->resource->height!=600)return E_NOTIMPL;
 hr=geo_typed_validate_native_state(h);if(FAILED(hr))return hr;
 if(!d->viewport_set||d->viewport.TopLeftX||d->viewport.TopLeftY||d->viewport.Width!=800||d->viewport.Height!=600||d->viewport.MinDepth||d->viewport.MaxDepth!=1)return E_NOTIMPL;
 for(i=0;i<24;i++){UINT v;memcpy(&v,(const BYTE *)vertices+4*i,4);if((v&0x7f800000u)==0x7f800000u)return E_INVALIDARG;}
 status=geo_pco_compile_pair(d->compiler.context,vs->tokens,vs->token_bytes,&vs->signatures,ps->tokens,ps->token_bytes,&ps->signatures,&pair);
 if(status!=GEO_OK)return status==GEO_BUSY?E_OUTOFMEMORY:(status==GEO_UNSUPPORTED?E_NOTIMPL:E_INVALIDARG);
 total=1024u+vs->token_bytes+ps->token_bytes+pair.vs.code_bytes+pair.ps.code_bytes;
 if(total>GEO_DRAW_MAX_BLOB||pair.vs.code_bytes>GEO_DRAW_MAX_CODE||pair.ps.code_bytes>GEO_DRAW_MAX_CODE||d->backend.command_bytes<sizeof(*p)+total||d->backend.allocation_count<2){hr=E_OUTOFMEMORY;goto done;}
 hr=geo_umd_target_info(&d->backend,&target,&ta);if(FAILED(hr))goto done;
 hr=geo_umd_write_private_vertex(&d->backend,&d->converted_color,vertices,96,&va);if(FAILED(hr))goto done;
 p=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*p)+total);if(!p){hr=E_OUTOFMEMORY;goto done;}blob=(BYTE *)(p+1);
 p->magic=GEO_DRAW_MAGIC;p->size=sizeof(*p)+total;p->version=2;p->vertex_index=1;
 p->request.base.size=sizeof(p->request);p->request.base.version=2;p->request.base.blob_bytes=total;p->request.base.target_desc=target;
 p->request.base.state.topology=GEO_DRAW_TRIANGLE_LIST;p->request.base.state.vertex_count=3;p->request.base.state.vertex_stride=32;p->request.base.state.color_write_mask=15;
 memcpy(p->request.base.state.viewport,&d->viewport,sizeof(d->viewport));p->request.link=pair.link;
 pair_shader(blob,&at,&p->request.base.vs,vs,&pair.vs);pair_shader(blob,&at,&p->request.base.ps,ps,&pair.ps);
 p->request.base.target=1;p->request.base.vertex=2;
 hr=geo_draw_validate_capture_v2(&p->request,blob,total)==GEO_OK?S_OK:E_INVALIDARG;p->request.base.target=p->request.base.vertex=0;
 if(FAILED(hr))goto done;
 memcpy(d->backend.commands,p,p->size);memset(d->backend.allocations,0,2*sizeof(*d->backend.allocations));
 d->backend.allocations[0].hAllocation=ta;d->backend.allocations[0].WriteOperation=1;d->backend.allocations[1].hAllocation=va;
 memset(&q,0,sizeof(q));q.hContext=d->backend.context;q.CommandLength=p->size;q.NumAllocations=2;
 hr=d->backend.cb.pfnRenderCb(d->backend.runtime,&q);geo_typed_device_trace(d,"PAIR_RENDER_HRESULT",(UINT)hr);
 if(SUCCEEDED(hr)){d->pending=1;d->backend.commands=q.pNewCommandBuffer;d->backend.command_bytes=q.NewCommandBufferSize;d->backend.allocations=q.pNewAllocationList;d->backend.allocation_count=q.NewAllocationListSize;d->backend.patches=q.pNewPatchLocationList;d->backend.patch_count=q.NewPatchLocationListSize;}
 done:
 /* RenderCb has synchronously captured tokens/USC; E owns independent job
  * copies and actual MDL refs. No user compiler storage backs pending DMA. */
 if(p)HeapFree(GetProcessHeap(),0,p);
 if(pair.vs.storage)d->compiler.release(d->compiler.context,&pair.vs);
 if(pair.ps.storage)d->compiler.release(d->compiler.context,&pair.ps);
 return hr;
}
