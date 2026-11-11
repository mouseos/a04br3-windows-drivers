/* SPDX-License-Identifier: MIT */
#include "bootstrap.h"
#include "shader_state.h"
#include "../adapter_info.h"
#include "adapter_info_trace.h"
#include "../frontend/draw_abi.h"
#include "../memory.h"
#include <d3d9caps.h>
struct adapter {struct geo_adapter_memory_info memory;};
struct declaration {struct declaration *next;UINT transformed,color,deleted;};
struct frontend {
 struct geo_shader_compiler compiler;void *compiler_device;
 struct geo_compiled_shader flat_vs,flat_ps;
 struct declaration *declarations,*decl;
 HANDLE vertex,index,converted;
 const BYTE *vertex_um,*index_um;
 UINT stride,offset,index_stride,rs[210];
 D3DDDIARG_VIEWPORTINFO viewport;UINT viewport_set;
 UINT min_z,max_z;
 RECT scissor;HRESULT last_submit;
};
static const UINT flat_vs_tokens[]={0xfffe0200u,0x0200001fu,0x80000000u,0x900f0000u,0x02000001u,0xc00f0000u,0x90e40000u,0x0000ffffu};
static const UINT flat_ps_tokens[]={0xffff0200u,0x02000001u,0x800f0800u,0xa0e40000u,0x0000ffffu};
static UINT bits(FLOAT f){UINT v;memcpy(&v,&f,4);return v;}
static FLOAT number(UINT v){FLOAT f;memcpy(&f,&v,4);return f;}
static int finite(FLOAT f){return (bits(f)&0x7f800000u)!=0x7f800000u;}
static struct frontend *state(struct geo_umd_device *d){return d?d->frontend_private:NULL;}
static HRESULT APIENTRY flush(HANDLE h) {
 struct frontend *s=state(h);return s?s->last_submit:E_INVALIDARG;
 /* Draw/Clear submit synchronously through real RenderCb. No CPU queue is
  * left here. This return means flushed, never GPU complete/idle/NoDMA. */
}
static HRESULT APIENTRY set_render_state(HANDLE h,const D3DDDIARG_RENDERSTATE *p) {
 struct frontend *s=state(h);UINT v;
 if(!s||!p||(UINT)p->State>=210u)return E_INVALIDARG;v=p->Value;
 switch(p->State) {
 case D3DDDIRS_ZENABLE:if(v>D3DZB_USEW)return E_INVALIDARG;break;
 case D3DDDIRS_ZWRITEENABLE:case D3DDDIRS_ALPHABLENDENABLE:case D3DDDIRS_ALPHATESTENABLE:
 case D3DDDIRS_STENCILENABLE:case D3DDDIRS_SCISSORTESTENABLE:case D3DDDIRS_FOGENABLE:
 case D3DDDIRS_LIGHTING:case D3DDDIRS_SPECULARENABLE:case D3DDDIRS_SRGBWRITEENABLE:
 case D3DDDIRS_SEPARATEALPHABLENDENABLE:case D3DDDIRS_MULTISAMPLEANTIALIAS:if(v>1u)return E_INVALIDARG;break;
 case D3DDDIRS_CULLMODE:if(v<D3DCULL_NONE||v>D3DCULL_CCW)return E_INVALIDARG;break;
 case D3DDDIRS_SHADEMODE:if(v!=D3DSHADE_FLAT&&v!=D3DSHADE_GOURAUD)return E_NOTIMPL;break;
 case D3DDDIRS_FILLMODE:if(v!=D3DFILL_SOLID)return E_NOTIMPL;break;
 case D3DDDIRS_COLORWRITEENABLE:if(v!=15u)return E_NOTIMPL;break;
 case D3DDDIRS_SRCBLEND:case D3DDDIRS_SRCBLENDALPHA:if(v!=D3DBLEND_ONE)return E_NOTIMPL;break;
 case D3DDDIRS_DESTBLEND:case D3DDDIRS_DESTBLENDALPHA:if(v!=D3DBLEND_ZERO)return E_NOTIMPL;break;
 case D3DDDIRS_BLENDOP:case D3DDDIRS_BLENDOPALPHA:if(v!=D3DBLENDOP_ADD)return E_NOTIMPL;break;
 default:return E_NOTIMPL;
 }
 s->rs[p->State]=v;return S_OK;
}
static HRESULT APIENTRY set_viewport(HANDLE h,const D3DDDIARG_VIEWPORTINFO *p) {
 struct frontend *s=state(h);
 if(!s||!p||!p->Width||!p->Height||p->Width>1024u||p->Height>1024u||p->X>1024u-p->Width||p->Y>1024u-p->Height)return E_INVALIDARG;
 s->viewport=*p;s->viewport_set=1;return S_OK;
}
static HRESULT APIENTRY set_z_range(HANDLE h,const D3DDDIARG_ZRANGE *p) {
 struct frontend *s=state(h);UINT lo,hi;
 if(!s||!p)return E_INVALIDARG;lo=bits(p->MinZ);hi=bits(p->MaxZ);
 if((lo|hi)&0x80000000u||lo>hi||hi>0x3f800000u)return E_INVALIDARG;
 s->min_z=lo;s->max_z=hi;return S_OK;
}
static HRESULT APIENTRY set_scissor(HANDLE h,const RECT *p) {
 struct frontend *s=state(h);if(!s||!p||p->left>p->right||p->top>p->bottom)return E_INVALIDARG;s->scissor=*p;return S_OK;
}
static HRESULT APIENTRY set_texture(HANDLE h,UINT stage,HANDLE resource) {
 if(!state(h)||stage>=16u)return E_INVALIDARG;
 /* No texture resources or samplers are advertised by this flat profile. */
 return resource?E_NOTIMPL:S_OK;
}
static HRESULT APIENTRY set_texture_stage(HANDLE h,const D3DDDIARG_TEXTURESTAGESTATE *p) {
 if(!state(h)||!p||p->Stage>=16u)return E_INVALIDARG;
 if((p->State==D3DDDITSS_COLOROP||p->State==D3DDDITSS_ALPHAOP)&&
    (p->Value==D3DTOP_DISABLE||p->Value==D3DTOP_SELECTARG1))return S_OK;
 if((p->State==D3DDDITSS_COLORARG1||p->State==D3DDDITSS_ALPHAARG1)&&p->Value==D3DTA_DIFFUSE)return S_OK;
 return E_NOTIMPL;
}
static HRESULT APIENTRY set_stream(HANDLE h,const D3DDDIARG_SETSTREAMSOURCE *p) {
 struct geo_umd_device *d=h;struct frontend *s=state(d);struct geo_surface_desc desc;D3DKMT_HANDLE a;HRESULT hr;
 if(!s||!p||p->Stream)return E_INVALIDARG;
 if(!p->hVertexBuffer){s->vertex=NULL;s->vertex_um=NULL;s->stride=s->offset=0;return S_OK;}
 hr=geo_umd_resource_info(d,p->hVertexBuffer,GEO_USAGE_VERTEX_BUFFER,&desc,&a);if(FAILED(hr))return hr;
 if(p->Stride<16u||p->Stride>64u||p->Offset>desc.pitch)return E_INVALIDARG;
 s->vertex=p->hVertexBuffer;s->vertex_um=NULL;s->stride=p->Stride;s->offset=p->Offset;return S_OK;
}
static HRESULT APIENTRY set_stream_um(HANDLE h,const D3DDDIARG_SETSTREAMSOURCEUM *p,const void *data) {
 struct frontend *s=state(h);if(!s||!p||p->Stream||!data||p->Stride<16u||p->Stride>64u)return E_INVALIDARG;
 s->vertex=NULL;s->vertex_um=data;s->stride=p->Stride;s->offset=0;return S_OK;
}
static HRESULT APIENTRY set_indices(HANDLE h,const D3DDDIARG_SETINDICES *p) {
 struct geo_umd_device *d=h;struct frontend *s=state(d);struct geo_surface_desc desc;D3DKMT_HANDLE a;HRESULT hr;
 if(!s||!p)return E_INVALIDARG;
 if(!p->hIndexBuffer){s->index=NULL;s->index_um=NULL;s->index_stride=0;return S_OK;}
 hr=geo_umd_resource_info(d,p->hIndexBuffer,GEO_USAGE_INDEX_BUFFER,&desc,&a);if(FAILED(hr))return hr;
 if((p->Stride!=2u&&p->Stride!=4u)||desc.format!=(p->Stride==2u?GEO_FORMAT_INDEX16:GEO_FORMAT_INDEX32))return E_INVALIDARG;
 s->index=p->hIndexBuffer;s->index_um=NULL;s->index_stride=p->Stride;return S_OK;
}
static HRESULT APIENTRY set_indices_um(HANDLE h,UINT stride,const void *data) {
 struct frontend *s=state(h);if(!s||!data||(stride!=2u&&stride!=4u))return E_INVALIDARG;
 s->index=NULL;s->index_um=data;s->index_stride=stride;return S_OK;
}
static struct declaration *lookup(struct frontend *s,HANDLE h) {
 struct declaration *p;for(p=s->declarations;p;p=p->next)if(p==(struct declaration *)h&&!p->deleted)return p;return NULL;
}
static HRESULT APIENTRY create_decl(HANDLE h,D3DDDIARG_CREATEVERTEXSHADERDECL *p,const D3DDDIVERTEXELEMENT *e) {
 struct frontend *s=state(h);struct declaration *decl;UINT i,position=0,color=0,transformed=0;
 if(!s||!p||!e||!p->NumVertexElements||p->NumVertexElements>3u)return E_INVALIDARG;
 for(i=0;i<p->NumVertexElements;i++) {
  if(e[i].Stream==0xffu&&e[i].Type==D3DDECLTYPE_UNUSED&&i+1u==p->NumVertexElements)break;
  if(e[i].Stream||e[i].Method!=D3DDECLMETHOD_DEFAULT||e[i].UsageIndex)return E_NOTIMPL;
  if(e[i].Type==D3DDECLTYPE_FLOAT4&&!e[i].Offset&&!position&&
     (e[i].Usage==D3DDECLUSAGE_POSITION||e[i].Usage==D3DDECLUSAGE_POSITIONT)){position=1;transformed=e[i].Usage==D3DDECLUSAGE_POSITIONT;}
  else if(e[i].Type==D3DDECLTYPE_D3DCOLOR&&e[i].Offset==16u&&e[i].Usage==D3DDECLUSAGE_COLOR&&!color)color=1;
  else return E_NOTIMPL;
 }
 if(!position)return E_NOTIMPL;
 decl=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*decl));if(!decl)return E_OUTOFMEMORY;
 decl->transformed=transformed;decl->color=color;decl->next=s->declarations;s->declarations=decl;p->ShaderHandle=decl;return S_OK;
}
static void collect_declarations(struct frontend *s) {
 struct declaration **link=&s->declarations,*p;while((p=*link)!=NULL){if(p->deleted&&p!=s->decl){*link=p->next;HeapFree(GetProcessHeap(),0,p);}else link=&p->next;}
}
static HRESULT APIENTRY set_decl(HANDLE h,HANDLE handle) {
 struct frontend *s=state(h);struct declaration *p=NULL;if(!s)return E_INVALIDARG;
 if(handle&&!(p=lookup(s,handle)))return E_INVALIDARG;s->decl=p;collect_declarations(s);return S_OK;
}
static HRESULT APIENTRY delete_decl(HANDLE h,HANDLE handle) {
 struct frontend *s=state(h);struct declaration *p;if(!s||!(p=lookup(s,handle)))return E_INVALIDARG;
 p->deleted=1;collect_declarations(s);return S_OK;
}
static HRESULT profile_state(struct frontend *s,const struct geo_surface_desc *target) {
 if(s->rs[D3DDDIRS_ZENABLE]||s->rs[D3DDDIRS_ZWRITEENABLE]||s->rs[D3DDDIRS_ALPHATESTENABLE]||
    s->rs[D3DDDIRS_STENCILENABLE]||s->rs[D3DDDIRS_FOGENABLE]||s->rs[D3DDDIRS_SPECULARENABLE]||s->rs[D3DDDIRS_SRGBWRITEENABLE])return E_NOTIMPL;
 (void)target; /* TL path clips the real logical/scissor bounds on CPU. */
 /* Identity ONE/ZERO ADD blend is exactly unblended native output. Other
  * factors were rejected by setters; it is not a general blend cap. */
 return S_OK;
}
static HRESULT read_vertex(struct geo_umd_device *d,struct frontend *s,UINT index,UINT byte_offset,BYTE out[64]) {
 uint64_t offset=(uint64_t)s->offset+(byte_offset?(uint64_t)index:(uint64_t)index*s->stride);
 if(!s->stride||offset>UINT32_MAX||(!byte_offset&&index>65534u))return E_INVALIDARG;
 if(s->vertex_um){if(offset>UINTPTR_MAX-s->stride||(uintptr_t)s->vertex_um>UINTPTR_MAX-(uintptr_t)offset-s->stride)return E_INVALIDARG;
  memcpy(out,s->vertex_um+(UINT)offset,s->stride);return S_OK;}
 return geo_umd_read_buffer(d,s->vertex,(UINT)offset,s->stride,out);
}
static void add_span(BYTE *blob,UINT *at,struct geo_draw_span *span,const void *data,UINT bytes) {
 span->offset=*at;span->bytes=bytes;memcpy(blob+*at,data,bytes);*at+=bytes;
}
static void shader_packet(struct geo_draw_shader *out,const struct geo_compiled_shader *in,UINT stage,
 BYTE *blob,UINT *at,const UINT *tokens,UINT token_bytes,const UINT *constants) {
 memset(out,0,sizeof(*out));out->format=GEO_SHADER_D3D9_SM2;out->stage=stage;
 out->temps=in->temps;out->shareds=in->shareds;out->vtxins=in->vtxins;out->vtxouts=in->vtxouts;
 out->input_mask=in->input_mask;out->constant_mask=in->constant_mask;
 add_span(blob,at,&out->tokens,tokens,token_bytes);add_span(blob,at,&out->usc,in->code,in->code_bytes);
 add_span(blob,at,&out->constants,constants,32u*16u);
}
static HRESULT emit_triangle(struct geo_umd_device *d,struct geo_surface_desc target,D3DKMT_HANDLE target_allocation,
 const FLOAT vertices[3][4],const struct geo_compiled_shader *vs,const struct geo_compiled_shader *ps,
 const UINT *vt,UINT vb,const UINT *pt,UINT pb,const UINT *vc,const UINT *pc) {
 struct frontend *s=state(d);struct geo_draw_packet *packet;D3DDDICB_RENDER q;D3DDDIARG_VIEWPORTINFO vp;
 D3DKMT_HANDLE vertex_allocation;UINT i,k,total,at=0;BYTE *blob;HRESULT hr;
 memset(&vp,0,sizeof(vp));vp.Width=target.width;vp.Height=target.height;
 for(i=0;i<3u;i++)for(k=0;k<4u;k++)if(!finite(vertices[i][k]))return E_INVALIDARG;
 if(vb>GEO_DRAW_MAX_TOKENS*4u||pb>GEO_DRAW_MAX_TOKENS*4u||vs->code_bytes>GEO_DRAW_MAX_CODE||ps->code_bytes>GEO_DRAW_MAX_CODE)return E_INVALIDARG;
 total=vb+pb+vs->code_bytes+ps->code_bytes+1024u;
 if(total>GEO_DRAW_MAX_BLOB||d->command_bytes<sizeof(*packet)+total||!d->commands||d->allocation_count<2u||!d->allocations)return E_OUTOFMEMORY;
 /* Real VidMm Lock serializes reuse of this input against its previous GPU
  * fence. Native output remains the original target MDL, never CPU-painted. */
 hr=geo_umd_write_private_vertex(d,&s->converted,vertices,12u*sizeof(FLOAT),&vertex_allocation);if(FAILED(hr))return hr;
 packet=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*packet)+total);if(!packet)return E_OUTOFMEMORY;blob=(BYTE *)(packet+1);
 packet->magic=GEO_DRAW_MAGIC;packet->size=sizeof(*packet)+total;packet->version=1;packet->vertex_index=1;
 packet->request.size=sizeof(packet->request);packet->request.version=1;packet->request.blob_bytes=total;packet->request.target_desc=target;
 packet->request.state.topology=GEO_DRAW_TRIANGLE_LIST;packet->request.state.vertex_count=3;packet->request.state.vertex_stride=16;
 packet->request.state.viewport[0]=bits((FLOAT)vp.X);packet->request.state.viewport[1]=bits((FLOAT)vp.Y);
 packet->request.state.viewport[2]=bits((FLOAT)vp.Width);packet->request.state.viewport[3]=bits((FLOAT)vp.Height);
 packet->request.state.viewport[4]=0;packet->request.state.viewport[5]=0x3f800000u;packet->request.state.color_write_mask=15;
 shader_packet(&packet->request.vs,vs,0,blob,&at,vt,vb,vc);shader_packet(&packet->request.ps,ps,1,blob,&at,pt,pb,pc);
 /* Locally validate the exact semantic capture with temporary private IDs;
  * wire IDs are reset to zero and assigned only by the kernel resolver. */
 packet->request.target=1;packet->request.vertex=2;
 hr=geo_draw_validate_capture(&packet->request,blob,total)==GEO_OK?S_OK:E_INVALIDARG;
 packet->request.target=packet->request.vertex=0;
 if(FAILED(hr)){HeapFree(GetProcessHeap(),0,packet);return hr;}
 memcpy(d->commands,packet,packet->size);memset(d->allocations,0,2u*sizeof(*d->allocations));
 d->allocations[0].hAllocation=target_allocation;d->allocations[0].WriteOperation=1;d->allocations[1].hAllocation=vertex_allocation;
 memset(&q,0,sizeof(q));q.hContext=d->context;q.CommandLength=packet->size;q.NumAllocations=2;
 hr=d->cb.pfnRenderCb(d->runtime,&q);HeapFree(GetProcessHeap(),0,packet);s->last_submit=hr;
 if(SUCCEEDED(hr)){d->commands=q.pNewCommandBuffer;d->command_bytes=q.NewCommandBufferSize;d->allocations=q.pNewAllocationList;d->allocation_count=q.NewAllocationListSize;d->patches=q.pNewPatchLocationList;d->patch_count=q.NewPatchLocationListSize;}
 return hr;
}
static HRESULT clip_triangle(const FLOAT input[3][4],const FLOAT bounds[6],FLOAT result[16][4],UINT *count) {
 FLOAT a[16][4],b[16][4];UINT n=3,plane,i,k,m;
 memcpy(a,input,3u*4u*sizeof(FLOAT));
 for(plane=0;plane<6u&&n;plane++){
  UINT axis=plane/2u,lower=!(plane&1u);FLOAT bound=bounds[plane];m=0;
  for(i=0;i<n;i++){
   const FLOAT *prev=a[(i+n-1u)%n],*cur=a[i];UINT pin=lower?prev[axis]>=bound:prev[axis]<=bound,cin=lower?cur[axis]>=bound:cur[axis]<=bound;
   if(pin!=cin){FLOAT t,denom=cur[axis]-prev[axis];if(!finite(denom)||denom==0.0f||m>=16u)return E_INVALIDARG;
    t=(bound-prev[axis])/denom;if(!finite(t))return E_INVALIDARG;
    for(k=0;k<4u;k++){b[m][k]=prev[k]+(cur[k]-prev[k])*t;}b[m][axis]=bound;m++;
   }
   if(cin){if(m>=16u)return E_INVALIDARG;memcpy(b[m++],cur,4u*sizeof(FLOAT));}
  }
  n=m;memcpy(a,b,n*4u*sizeof(FLOAT));
 }
 memcpy(result,a,n*4u*sizeof(FLOAT));*count=n;return S_OK;
}
static HRESULT draw_vertices(struct geo_umd_device *d,const UINT indices[3],UINT byte_offsets,UINT force_tl) {
 struct frontend *s=state(d);struct geo_surface_desc target;D3DKMT_HANDLE target_allocation;
 D3DDDIARG_VIEWPORTINFO vp;
 const struct geo_compiled_shader *vs,*ps;const UINT *vc,*pc,*vt,*pt;UINT vb=0,pb=0,mask,logical_width,logical_height,flat_constants[128]={0},zero_constants[128]={0};
 FLOAT vertices[3][4],positions[3][4],area;UINT colors[3],i,k;BYTE raw[64];HRESULT hr;
 if(!s||!s->decl)return E_INVALIDARG;
 hr=geo_umd_target_info(d,&target,&target_allocation);if(FAILED(hr))return hr;
 hr=geo_umd_target_extent(d,&logical_width,&logical_height);if(FAILED(hr))return hr;
 hr=profile_state(s,&target);if(FAILED(hr))return hr;
 vp=s->viewport;if(!s->viewport_set){memset(&vp,0,sizeof(vp));vp.Width=logical_width;vp.Height=logical_height;}
 if(vp.X>logical_width||vp.Width>logical_width-vp.X||vp.Y>logical_height||vp.Height>logical_height-vp.Y)return E_INVALIDARG;
 for(i=0;i<3u;i++){
  hr=read_vertex(d,s,indices[i],byte_offsets,raw);if(FAILED(hr))return hr;
  memcpy(positions[i],raw,16u);colors[i]=0xffffffffu;if(s->decl->color){if(s->stride<20u)return E_INVALIDARG;memcpy(&colors[i],raw+16,4u);}
  for(k=0;k<4u;k++)if(!finite(positions[i][k]))return E_INVALIDARG;
 }
 if(force_tl||s->decl->transformed) {
  area=(positions[1][0]-positions[0][0])*(positions[2][1]-positions[0][1])-(positions[1][1]-positions[0][1])*(positions[2][0]-positions[0][0]);
  if(!finite(area))return E_INVALIDARG;
  if(area==0.0f||(s->rs[D3DDDIRS_CULLMODE]==D3DCULL_CW&&area>0.0f)||
     (s->rs[D3DDDIRS_CULLMODE]==D3DCULL_CCW&&area<0.0f))return S_OK; /* truly culled geometry */
  if(s->rs[D3DDDIRS_SHADEMODE]!=D3DSHADE_FLAT&&(colors[0]!=colors[1]||colors[0]!=colors[2]))return E_NOTIMPL;
  for(k=0;k<3u;k++){FLOAT color=(FLOAT)((colors[0]>>(16u-8u*k))&255u)/255.0f;flat_constants[k]=bits(color);}flat_constants[3]=bits((FLOAT)((colors[0]>>24)&255u)/255.0f);
  {
   FLOAT polygon[16][4],bounds[6];UINT n,j;
   bounds[0]=(FLOAT)vp.X;bounds[1]=(FLOAT)(vp.X+vp.Width);bounds[2]=(FLOAT)vp.Y;bounds[3]=(FLOAT)(vp.Y+vp.Height);
   bounds[4]=number(s->min_z);bounds[5]=number(s->max_z);
   if(s->rs[D3DDDIRS_SCISSORTESTENABLE]){
    if((FLOAT)s->scissor.left>bounds[0])bounds[0]=(FLOAT)s->scissor.left;
    if((FLOAT)s->scissor.right<bounds[1])bounds[1]=(FLOAT)s->scissor.right;
    if((FLOAT)s->scissor.top>bounds[2])bounds[2]=(FLOAT)s->scissor.top;
    if((FLOAT)s->scissor.bottom<bounds[3])bounds[3]=(FLOAT)s->scissor.bottom;
   }
   if(bounds[0]>=bounds[1]||bounds[2]>=bounds[3])return S_OK;
   if(s->max_z==s->min_z)return E_NOTIMPL;
   hr=clip_triangle(positions,bounds,polygon,&n);if(FAILED(hr))return hr;
   for(j=1u;j+1u<n;j++){
    const FLOAT *p[3]={polygon[0],polygon[j],polygon[j+1u]};
    for(i=0;i<3u;i++){
     FLOAT w;if(p[i][3]<=0.0f)return E_INVALIDARG;w=1.0f/p[i][3];
     /* D3D9 pixel centres are integer; native_triangle_suite.c uses .5.
      * native_triangle_ppp.c has +400/+300 and .5/.5 scale/offset.
      * Logical RTs share the same padded 800x600 VidMm target, never a
      * separate private output. CPU clipping limits logical viewport writes. */
     vertices[i][0]=((p[i][0]+0.5f)*2.0f/(FLOAT)target.width-1.0f)*w;
     vertices[i][1]=((p[i][1]+0.5f)*2.0f/(FLOAT)target.height-1.0f)*w;
     vertices[i][2]=((p[i][2]-number(s->min_z))*2.0f/(number(s->max_z)-number(s->min_z))-1.0f)*w;vertices[i][3]=w;
    }
    hr=emit_triangle(d,target,target_allocation,vertices,&s->flat_vs,&s->flat_ps,flat_vs_tokens,sizeof(flat_vs_tokens),flat_ps_tokens,sizeof(flat_ps_tokens),zero_constants,flat_constants);
    if(FAILED(hr))return hr;
   }
   return S_OK;
  }
 } else {
  if(vp.X||vp.Y||vp.Width!=target.width||vp.Height!=target.height||s->min_z||s->max_z!=0x3f800000u)return E_NOTIMPL;
  if(s->rs[D3DDDIRS_CULLMODE]!=D3DCULL_NONE)return E_NOTIMPL;
  hr=geo_umd_shader_snapshot(d,0,&vs,&vc,&mask);if(FAILED(hr))return hr;
  hr=geo_umd_shader_snapshot(d,1,&ps,&pc,&mask);if(FAILED(hr))return hr;
  hr=geo_umd_shader_tokens(d,0,&vt,&vb);if(FAILED(hr))return hr;hr=geo_umd_shader_tokens(d,1,&pt,&pb);if(FAILED(hr))return hr;
  memcpy(vertices,positions,sizeof(vertices));
 }
 return emit_triangle(d,target,target_allocation,vertices,vs,ps,vt,vb,pt,pb,vc,pc);
}

/* Direct3D9 triangle order/provoking vertex follows Microsoft's triangle
 * strips/fans specification. Fan vertex zero is the anchor, not the flat
 * color source. The strip swaps only its final two vertices on odd triangles.
 * Every emitted primitive reuses the same VidMm input via a real Lock fence;
 * no CPU rasterization or fabricated GPU completion is introduced. */
static HRESULT triangle_slots(UINT topology,UINT count,UINT triangle,UINT slots[3]) {
 if(!count||count>65535u||triangle>=count)return E_INVALIDARG;
 if(topology==D3DPT_TRIANGLELIST){slots[0]=3u*triangle;slots[1]=slots[0]+1u;slots[2]=slots[0]+2u;}
 else if(topology==D3DPT_TRIANGLESTRIP){slots[0]=triangle;slots[1]=triangle+1u+(triangle&1u);slots[2]=triangle+2u-(triangle&1u);}
 else if(topology==D3DPT_TRIANGLEFAN){slots[0]=triangle+1u;slots[1]=triangle+2u;slots[2]=0;}
 else return E_NOTIMPL;
 return S_OK;
}
static HRESULT APIENTRY draw_primitive(HANDLE h,const D3DDDIARG_DRAWPRIMITIVE *p,const UINT *edge_flags) {
 UINT indices[3],j,i;HRESULT hr;(void)edge_flags;
 if(!state(h)||!p)return E_INVALIDARG;
 for(j=0;j<p->PrimitiveCount;j++){
  hr=triangle_slots(p->PrimitiveType,p->PrimitiveCount,j,indices);if(FAILED(hr))return hr;
  for(i=0;i<3u;i++){if(p->VStart>65534u||indices[i]>65534u-p->VStart)return E_INVALIDARG;indices[i]+=p->VStart;}
  hr=draw_vertices(h,indices,0,0);if(FAILED(hr))return hr;
 }
 return p->PrimitiveCount?S_OK:E_INVALIDARG;
}
static HRESULT APIENTRY draw_indexed(HANDLE h,const D3DDDIARG_DRAWINDEXEDPRIMITIVE *p) {
 struct frontend *s=state(h);UINT slots[3],indices[3],i,j,offset;HRESULT hr;BYTE raw[4];
 if(!s||!p||!s->index_stride)return E_INVALIDARG;
 for(j=0;j<p->PrimitiveCount;j++){
  hr=triangle_slots(p->PrimitiveType,p->PrimitiveCount,j,slots);if(FAILED(hr))return hr;
  for(i=0;i<3u;i++){
   int64_t vertex;UINT v=0;
   if(p->StartIndex>UINT32_MAX/s->index_stride||slots[i]>UINT32_MAX/s->index_stride-p->StartIndex)return E_INVALIDARG;
   offset=(p->StartIndex+slots[i])*s->index_stride;
   if(s->index_um){if(offset>UINTPTR_MAX-s->index_stride||(uintptr_t)s->index_um>UINTPTR_MAX-offset-s->index_stride)return E_INVALIDARG;memcpy(raw,s->index_um+offset,s->index_stride);}
   else {hr=geo_umd_read_buffer(h,s->index,offset,s->index_stride,raw);if(FAILED(hr))return hr;}
   if(s->index_stride==2u){USHORT v16;memcpy(&v16,raw,2);v=v16;}else memcpy(&v,raw,4);
   if(v<p->MinIndex||v-p->MinIndex>=p->NumVertices)return E_INVALIDARG;
   vertex=(int64_t)p->BaseVertexIndex+v;if(vertex<0||vertex>65534)return E_INVALIDARG;indices[i]=(UINT)vertex;
  }
  hr=draw_vertices(h,indices,0,0);if(FAILED(hr))return hr;
 }
 return p->PrimitiveCount?S_OK:E_INVALIDARG;
}
static HRESULT APIENTRY draw_primitive2(HANDLE h,const D3DDDIARG_DRAWPRIMITIVE2 *p) {
 struct frontend *s=state(h);UINT offsets[3],j,i;HRESULT hr;
 if(!s||!p||!s->stride)return E_INVALIDARG;
 for(j=0;j<p->PrimitiveCount;j++){
  hr=triangle_slots(p->PrimitiveType,p->PrimitiveCount,j,offsets);if(FAILED(hr))return hr;
  for(i=0;i<3u;i++){
   if(offsets[i]>(UINT32_MAX-p->FirstVertexOffset)/s->stride)return E_INVALIDARG;
   offsets[i]=p->FirstVertexOffset+offsets[i]*s->stride;
  }
  hr=draw_vertices(h,offsets,1,1);if(FAILED(hr))return hr;
 }
 return p->PrimitiveCount?S_OK:E_INVALIDARG;
}
static HRESULT APIENTRY draw_indexed2(HANDLE h,const D3DDDIARG_DRAWINDEXEDPRIMITIVE2 *p,UINT stride,const void *data,const UINT *edge_flags) {
 struct frontend *s=state(h);UINT slots[3],offsets[3],i,j;const BYTE *raw;HRESULT hr;
 (void)edge_flags;
 if(!s||!p||!data||(stride!=2u&&stride!=4u))return E_INVALIDARG;
 for(j=0;j<p->PrimitiveCount;j++){
  hr=triangle_slots(p->PrimitiveType,p->PrimitiveCount,j,slots);if(FAILED(hr))return hr;
  for(i=0;i<3u;i++){
   UINT v=0;uint64_t byte_offset=(uint64_t)p->StartIndexOffset+(uint64_t)slots[i]*stride;int64_t offset;
   if(byte_offset>UINTPTR_MAX-stride||(uintptr_t)data>UINTPTR_MAX-(UINT)byte_offset-stride)return E_INVALIDARG;
   raw=(const BYTE *)data+(UINT)byte_offset;
   if(stride==2u){USHORT n;memcpy(&n,raw,2);v=n;}else memcpy(&v,raw,4);
   if(v<p->MinIndex||v-p->MinIndex>=p->NumVertices||v>65534u)return E_INVALIDARG;
   offset=(int64_t)p->BaseVertexOffset+(int64_t)v*s->stride;
   if(offset<0||offset>UINT32_MAX-s->stride)return E_INVALIDARG;offsets[i]=(UINT)offset;
  }
  hr=draw_vertices(h,offsets,1,1);if(FAILED(hr))return hr;
 }
 return p->PrimitiveCount?S_OK:E_INVALIDARG;
}
static HRESULT APIENTRY set_depth_stencil(HANDLE h,const D3DDDIARG_SETDEPTHSTENCIL *p) {
 if(!state(h)||!p)return E_INVALIDARG;return p->hZBuffer?E_NOTIMPL:S_OK;
}
static HRESULT APIENTRY validate_device(HANDLE h,D3DDDIARG_VALIDATETEXTURESTAGESTATE *p) {
 struct geo_surface_desc target;D3DKMT_HANDLE allocation;HRESULT hr;
 if(!state(h)||!p)return E_INVALIDARG;hr=geo_umd_target_info(h,&target,&allocation);if(FAILED(hr))return hr;
 hr=profile_state(state(h),&target);if(FAILED(hr))return hr;p->NumPasses=1;return S_OK;
}
static HRESULT open_adapter(const D3DDDIARG_OPENADAPTER *p,void **out) {
 struct adapter *a;D3DDDICB_QUERYADAPTERINFO q;HRESULT hr;
 if(!p||!out)return E_INVALIDARG;*out=NULL;
 a=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*a));if(!a)return E_OUTOFMEMORY;
 memset(&q,0,sizeof(q));q.pPrivateDriverData=&a->memory;q.PrivateDriverDataSize=sizeof(a->memory);
 hr=p->pAdapterCallbacks->pfnQueryAdapterInfoCb(p->hAdapter,&q);
 if(FAILED(hr)){HeapFree(GetProcessHeap(),0,a);return hr;}
 {uint32_t mismatch=geo_adapter_wire_mismatch(&a->memory);
  if(mismatch){geo_umd_wire_trace(&a->memory,mismatch);HeapFree(GetProcessHeap(),0,a);return E_UNEXPECTED;}}
 *out=a;return S_OK;
}
static HRESULT get_caps(void *context,const D3DDDIARG_GETCAPS *p) {
 struct adapter *a=context;
 if(!a||!p||!p->pData)return E_INVALIDARG;
 switch(p->Type){
 case D3DDDICAPS_DDRAW:if(p->DataSize!=sizeof(DDRAW_CAPS))return E_INVALIDARG;memset(p->pData,0,p->DataSize);return S_OK;
 case D3DDDICAPS_DDRAW_MODE_SPECIFIC:{DDRAW_MODE_SPECIFIC_CAPS *m=p->pData;UINT head;if(p->DataSize!=sizeof(*m))return E_INVALIDARG;head=m->Head;memset(m,0,sizeof(*m));m->Head=head;return S_OK;}
 case D3DDDICAPS_GETFORMATCOUNT:if(p->DataSize!=sizeof(UINT))return E_INVALIDARG;*(UINT *)p->pData=1;return S_OK;
 case D3DDDICAPS_GETD3DQUERYCOUNT:if(p->DataSize!=sizeof(UINT))return E_INVALIDARG;*(UINT *)p->pData=0;return S_OK;
 case D3DDDICAPS_GETFORMATDATA:{FORMATOP *f=p->pData;if(p->DataSize!=sizeof(*f))return E_INVALIDARG;memset(f,0,sizeof(*f));f->Format=D3DDDIFMT_A8R8G8B8;
  f->Operations=FORMATOP_OFFSCREEN_RENDERTARGET|FORMATOP_SAME_FORMAT_RENDERTARGET|FORMATOP_OFFSCREENPLAIN;f->BltMsTypes=1;return S_OK;}
 case D3DDDICAPS_GETD3D9CAPS:{D3DCAPS9 *c=p->pData;if(p->DataSize!=sizeof(*c))return E_INVALIDARG;memset(c,0,sizeof(*c));
  c->DeviceType=D3DDEVTYPE_HAL;c->DevCaps=D3DDEVCAPS_TLVERTEXSYSTEMMEMORY|D3DDEVCAPS_TLVERTEXVIDEOMEMORY|D3DDEVCAPS_DRAWPRIMTLVERTEX|D3DDEVCAPS_HWRASTERIZATION;
  c->PrimitiveMiscCaps=D3DPMISCCAPS_CULLNONE|D3DPMISCCAPS_CULLCW|D3DPMISCCAPS_CULLCCW;
  c->SrcBlendCaps=D3DPBLENDCAPS_ONE;c->DestBlendCaps=D3DPBLENDCAPS_ZERO;
  c->TextureOpCaps=D3DTEXOPCAPS_DISABLE|D3DTEXOPCAPS_SELECTARG1;c->MaxTextureBlendStages=1;
  c->RasterCaps=D3DPRASTERCAPS_SCISSORTEST;c->MaxTextureWidth=800;c->MaxTextureHeight=600;
  c->MaxPrimitiveCount=65535;c->MaxVertexIndex=65534;c->NumSimultaneousRTs=1;c->MaxStreams=1;c->MaxStreamStride=64;c->MaxVertexW=1.0e10f;c->MaxPointSize=1.0f;
  /* Legacy TL/flat profile, not FL9_1. Flat shading is implicit in D3D9
   * ShadeCaps; no Gouraud/texture/shader-version cap is fabricated. */
  return S_OK;}
 default:return E_NOTIMPL;
 }
}
static HRESULT initialize_device(void *context,struct geo_umd_device *d,D3DDDI_DEVICEFUNCS *f) {
 struct adapter *a=context;struct frontend *s;struct geo_sm2_error error;HRESULT hr;int32_t status;
 if(!a||!d||!f)return E_INVALIDARG;
 s=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*s));if(!s)return E_OUTOFMEMORY;
 status=geo_pco_device_create(a->memory.compiler_bvnc,&s->compiler,&s->compiler_device);
 if(status!=GEO_OK){HeapFree(GetProcessHeap(),0,s);return status==GEO_BUSY?E_OUTOFMEMORY:E_NOTIMPL;}
 status=s->compiler.compile(s->compiler.context,0,flat_vs_tokens,sizeof(flat_vs_tokens),&s->flat_vs,&error);
 if(status==GEO_OK)status=s->compiler.compile(s->compiler.context,1,flat_ps_tokens,sizeof(flat_ps_tokens),&s->flat_ps,&error);
 if(status!=GEO_OK){if(s->flat_vs.storage)s->compiler.release(s->compiler.context,&s->flat_vs);geo_pco_device_destroy(&s->compiler,s->compiler_device);HeapFree(GetProcessHeap(),0,s);return E_NOTIMPL;}
 hr=geo_umd_shader_init(d,&s->compiler);
 if(FAILED(hr)){s->compiler.release(s->compiler.context,&s->flat_vs);s->compiler.release(s->compiler.context,&s->flat_ps);geo_pco_device_destroy(&s->compiler,s->compiler_device);HeapFree(GetProcessHeap(),0,s);return hr;}
 s->rs[D3DDDIRS_ZENABLE]=D3DZB_TRUE;s->rs[D3DDDIRS_ZWRITEENABLE]=TRUE;s->rs[D3DDDIRS_CULLMODE]=D3DCULL_CCW;
 s->rs[D3DDDIRS_SHADEMODE]=D3DSHADE_GOURAUD;s->rs[D3DDDIRS_FILLMODE]=D3DFILL_SOLID;s->rs[D3DDDIRS_COLORWRITEENABLE]=15;
 s->rs[D3DDDIRS_SRCBLEND]=s->rs[D3DDDIRS_SRCBLENDALPHA]=D3DBLEND_ONE;s->rs[D3DDDIRS_DESTBLEND]=s->rs[D3DDDIRS_DESTBLENDALPHA]=D3DBLEND_ZERO;
 s->rs[D3DDDIRS_BLENDOP]=s->rs[D3DDDIRS_BLENDOPALPHA]=D3DBLENDOP_ADD;s->max_z=0x3f800000u;s->last_submit=S_OK;
 d->frontend_private=s;geo_umd_install_shader_ddis(f);
 f->pfnSetRenderState=set_render_state;f->pfnSetViewport=set_viewport;f->pfnSetZRange=set_z_range;f->pfnSetScissorRect=set_scissor;
 f->pfnSetTexture=set_texture;f->pfnSetTextureStageState=set_texture_stage;f->pfnValidateDevice=validate_device;
 f->pfnSetStreamSource=set_stream;f->pfnSetStreamSourceUm=set_stream_um;f->pfnSetIndices=set_indices;f->pfnSetIndicesUm=set_indices_um;
 f->pfnCreateVertexShaderDecl=create_decl;f->pfnSetVertexShaderDecl=set_decl;f->pfnDeleteVertexShaderDecl=delete_decl;
 f->pfnSetDepthStencil=set_depth_stencil;
 f->pfnDrawPrimitive=draw_primitive;f->pfnDrawIndexedPrimitive=draw_indexed;f->pfnDrawPrimitive2=draw_primitive2;f->pfnDrawIndexedPrimitive2=draw_indexed2;f->pfnFlush=flush;return S_OK;
}
static HRESULT quiesce_device(struct geo_umd_device *d) {
 struct frontend *s=state(d);HRESULT hr;if(!s)return E_INVALIDARG;
 hr=flush(d);if(FAILED(hr))return hr;
 hr=geo_umd_destroy_private_vertex(d,&s->converted);if(FAILED(hr))return hr;
 return geo_umd_shader_shutdown(d);
}
static void destroy_device(struct geo_umd_device *d) {
 struct frontend *s=state(d);struct declaration *p,*next;if(!s)return;
 s->compiler.release(s->compiler.context,&s->flat_vs);s->compiler.release(s->compiler.context,&s->flat_ps);
 for(p=s->declarations;p;p=next){next=p->next;HeapFree(GetProcessHeap(),0,p);}
 geo_pco_device_destroy(&s->compiler,s->compiler_device);HeapFree(GetProcessHeap(),0,s);d->frontend_private=NULL;
}
static void close_adapter(void *context){HeapFree(GetProcessHeap(),0,context);}
const struct geo_umd_frontend geo_ge8300_frontend={sizeof(struct geo_umd_frontend),1,open_adapter,get_caps,initialize_device,quiesce_device,destroy_device,close_adapter};
