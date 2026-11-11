#!/usr/bin/env python3
"""Use the existing source-extraction host-fixture approach for actual IA callbacks.
Only OS/DDI handles, mapping and the final GPU submit are mocked. No GPU claim.
"""
import argparse, hashlib, json, subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
d=Path(__file__).resolve().parents[1];out=a.output.resolve()
if out.exists():raise ValueError('Fresh output required')
out.mkdir(parents=True)
s=(d/'umd/typed_draw.c').read_text();r=(d/'umd/typed_resource.c').read_text()
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t UINT;typedef int32_t INT,HRESULT;typedef uint16_t USHORT;typedef uint8_t BYTE;typedef float FLOAT;typedef size_t SIZE_T;
#define APIENTRY
#define S_OK 0
#define E_INVALIDARG ((HRESULT)-1)
#define E_NOTIMPL ((HRESULT)-2)
#define E_OUTOFMEMORY ((HRESULT)-3)
#define FAILED(x) ((x)<0)
#define HEAP_ZERO_MEMORY 1
#define GetProcessHeap() NULL
#define HeapFree(h,f,p) free(p)
#define HeapAlloc(h,f,n) calloc(1,n)
typedef struct {void*pDrvPrivate;} D3D10DDI_HDEVICE,D3D10DDI_HELEMENTLAYOUT,D3D10DDI_HRTELEMENTLAYOUT,D3D10DDI_HRESOURCE;
typedef enum {DXGI_FORMAT_R32G32B32A32_FLOAT=2,DXGI_FORMAT_R32G32B32_FLOAT=6,DXGI_FORMAT_R32G32_FLOAT=16,DXGI_FORMAT_R32_FLOAT=41,DXGI_FORMAT_R16_UINT=57,DXGI_FORMAT_R32_UINT=42,DXGI_FORMAT_B8G8R8A8_UNORM=87,DXGI_FORMAT_R8G8B8A8_UNORM=28} DXGI_FORMAT;
#define D3D11_1DDI_FORMAT_SUPPORT_VERTEX_BUFFER 1
#define D3D10_DDI_FORMAT_SUPPORT_RENDERTARGET 2
#define D3D10_DDI_FORMAT_SUPPORT_SHADER_SAMPLE 4
#define D3D10_DDI_INPUT_PER_VERTEX_DATA 0
#define D3D10_DDI_BIND_VERTEX_BUFFER 1
#define D3D10_DDI_BIND_INDEX_BUFFER 2
#define D3D10DDIRESOURCE_BUFFER 1
#define D3D10_DDI_MAP_READ 1
#define D3D10_DDI_PRIMITIVE_TOPOLOGY_UNDEFINED 0
#define D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST 4
#define D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP 5
typedef UINT D3D10_DDI_PRIMITIVE_TOPOLOGY;
typedef struct {UINT InputSlot,AlignedByteOffset;DXGI_FORMAT Format;UINT InputSlotClass,InstanceDataStepRate,InputRegister;} D3D10DDIARG_INPUT_ELEMENT_DESC;
typedef struct {const D3D10DDIARG_INPUT_ELEMENT_DESC*pVertexElements;UINT NumElements;} D3D10DDIARG_CREATEELEMENTLAYOUT;
typedef struct {void*pData;} D3D10DDI_MAPPED_SUBRESOURCE;
struct geo_typed_layout;
struct geo_typed_resource {UINT bind,bytes,dimension,refs;BYTE*data;};
struct geo_typed_handle {void*object;};
struct code {UINT pair_stage;};struct shader {struct code*code;};
struct geo_typed_device {struct geo_typed_layout *layout,*layouts;struct geo_typed_resource*vertex,*index;UINT vertex_stride,vertex_offset,index_stride,index_offset,topology;struct shader*bound_shader[2];HRESULT error;};
static struct geo_typed_device*geo_typed_device(D3D10DDI_HDEVICE h){return h.pDrvPrivate;}
static void geo_typed_error(D3D10DDI_HDEVICE h,HRESULT hr){((struct geo_typed_device*)h.pDrvPrivate)->error=hr;}
static struct geo_typed_resource*geo_typed_resource(D3D10DDI_HDEVICE h,D3D10DDI_HRESOURCE r){(void)h;return r.pDrvPrivate;}
static void geo_typed_resource_ref(struct geo_typed_resource*r){r->refs++;}
static HRESULT geo_typed_resource_unref(struct geo_typed_resource*r){assert(r->refs);r->refs--;return 0;}
static unsigned maps,submits;static HRESULT map_failure,unmap_failure;static uint32_t captured[3][8];
static HRESULT map_backend(struct geo_typed_resource*r,UINT how,UINT flags,D3D10DDI_MAPPED_SUBRESOURCE*m){(void)how;(void)flags;maps++;m->pData=r->data;return map_failure;}
static HRESULT unmap_backend(struct geo_typed_resource*r){(void)r;return unmap_failure;}
static HRESULT geo_typed_submit_draw(D3D10DDI_HDEVICE h,const FLOAT v[3][4]){(void)h;submits++;for(unsigned i=0;i<3;i++)memcpy(captured[i],v[i],16);return 0;}
static HRESULT geo_typed_submit_pair(D3D10DDI_HDEVICE h,const FLOAT v[3][8]){(void)h;submits++;memcpy(captured,v,sizeof(captured));return 0;}
'''
entry=(d/'umd/typed_entry.c').read_text()
code+=entry[entry.index('static void APIENTRY check_format('):entry.index('static void APIENTRY check_samples(')]
code+=r[r.index('HRESULT geo_typed_read_resource('):r.index('/* Synchronous CPU upload')]
code+=s[s.index('struct geo_typed_layout {'):s.index('static void APIENTRY set_viewports(')]
code+=s[s.index('static HRESULT read_vertex('):s.index('HRESULT geo_typed_draw_unbind(')]
# v/255 is normal for every nonzero byte. Compute the exact binary32
# rounding using integers only, independent of the production float division.
def unorm_bits(v):
    if not v:return 0
    exponent=0;scaled=v
    while scaled<255:scaled*=2;exponent-=1
    q,rem=divmod(v << (23-exponent),255)
    if 2*rem>255 or (2*rem==255 and q&1):q+=1
    return ((exponent+127)<<23)+(q-(1<<23))
code+='static const uint32_t unorm_expected[256]={'+','.join(hex(unorm_bits(v))+'u' for v in range(256))+'};\n'
code+=r'''
static struct geo_typed_device dev;static D3D10DDI_HDEVICE h={&dev};
static struct code codes[2];static struct shader shaders[2]={{codes},{codes+1}};
static struct geo_typed_handle lh;
static D3D10DDI_HELEMENTLAYOUT lhandle={&lh};
static const DXGI_FORMAT formats[4]={DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32G32_FLOAT,DXGI_FORMAT_R32G32B32_FLOAT,DXGI_FORMAT_R32G32B32A32_FLOAT};
static void install_formats(DXGI_FORMAT first,DXGI_FORMAT second,unsigned offset){
 D3D10DDIARG_INPUT_ELEMENT_DESC e[2]={{0,offset,first,0,0,0},{0,offset+layout_width(first),second,0,0,1}};
 D3D10DDIARG_CREATEELEMENTLAYOUT p={e,second?2:1};
 if(lh.object){set_layout(h,(D3D10DDI_HELEMENTLAYOUT){0});destroy_layout(h,lhandle);}
 assert(layout_size(h,&p)==sizeof(lh));create_layout(h,&p,lhandle,(D3D10DDI_HRTELEMENTLAYOUT){0});assert(lh.object);set_layout(h,lhandle);
 codes[0].pair_stage=second?1:0;codes[1].pair_stage=second?2:0;
 dev.bound_shader[0]=shaders;dev.bound_shader[1]=shaders+1;
 dev.error=0;submits=0;
}
static void install(unsigned components,unsigned second,unsigned offset){install_formats(formats[components-1],second?formats[second-1]:0,offset);}
static void bind(struct geo_typed_resource*r,UINT stride,UINT offset){D3D10DDI_HRESOURCE rh={r};set_vertices(h,0,1,&rh,&stride,&offset);}
static void expect(unsigned vertex,unsigned reg,unsigned components,const uint32_t*source){
 const uint32_t defaults[4]={0,0,0,0x3f800000};
 for(unsigned i=0;i<4;i++)assert(captured[vertex][reg*4+i]==(i<components?source[i]:defaults[i]));
}
int main(void){
 set_topology(h,D3D10_DDI_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 UINT support;for(unsigned f=0;f<4;f++){check_format(h,formats[f],&support);assert(support==D3D11_1DDI_FORMAT_SUPPORT_VERTEX_BUFFER);}
 check_format(h,DXGI_FORMAT_R32_UINT,&support);assert(!support);
 check_format(h,DXGI_FORMAT_B8G8R8A8_UNORM,&support);assert(support==6);
 check_format(h,DXGI_FORMAT_R8G8B8A8_UNORM,&support);assert(support==5);
 for(unsigned n=1;n<=4;n++){
  install(n,0,0);
  /* Exact resource end: ASAN catches a fixed 16-byte read for float1/2/3. */
  BYTE*data=malloc(3*n*4);struct geo_typed_resource vb={1,3*n*4,1,1,data};
  for(unsigned i=0;i<3*n;i++){uint32_t x=0x3f000000+i;memcpy(data+i*4,&x,4);}
  bind(&vb,n*4,0);assert(!dev.error);draw_vertices(h,3,0);assert(!dev.error&&submits==1);
  for(unsigned v=0;v<3;v++)expect(v,0,n,(uint32_t*)data+v*n);
  /* Zero stride and overlapping stride<width are valid bindings. */
  bind(&vb,0,0);submits=0;draw_vertices(h,3,0);assert(!dev.error&&submits==1);
  for(unsigned v=0;v<3;v++)expect(v,0,n,(uint32_t*)data);
  bind(&vb,4,0);submits=0;draw_vertices(h,3,0);assert(!dev.error&&submits==1);
  for(unsigned v=0;v<3;v++)expect(v,0,n,(uint32_t*)data+v);
  /* One byte short must fail through the production resource reader. */
  vb.bytes=n*4-1;bind(&vb,0,0);submits=0;draw_vertices(h,3,0);assert(dev.error==E_INVALIDARG&&!submits);vb.bytes=3*n*4;dev.error=0;
  /* Scalar copying must not canonicalize -0, signaling NaN or infinity. */
  uint32_t raw[4]={0x80000000,0x7f812345,0x7f800000,0xff800000},got[4];FLOAT result[4];
  memcpy(data,raw,n*4);assert(!read_vertex(&dev,0,0,result));memcpy(got,result,16);
  for(unsigned i=0;i<4;i++)assert(got[i]==(i<n?raw[i]:i==3?0x3f800000:0));
  map_failure=-9;assert(read_vertex(&dev,0,0,result)==-9);map_failure=0;
  unmap_failure=-10;assert(read_vertex(&dev,0,0,result)==-10);unmap_failure=0;
  bind(NULL,0,0);assert(vb.refs==1);free(data);
 }
 for(unsigned n=1;n<=4;n++)for(unsigned m=1;m<=4;m++){
  install(n,m,4);uint32_t data[32];for(unsigned i=0;i<32;i++)data[i]=0x3f000000+i;
  unsigned stride=(n+m+2)*4;struct geo_typed_resource vb={1,3*stride,1,1,(BYTE*)data};
  /* Allocate enough backing for padded records. */
  vb.data=calloc(3,stride);for(unsigned v=0;v<3;v++){memcpy(vb.data+v*stride,data,stride);uint32_t marker=0x3f100000+v;memcpy(vb.data+v*stride+8,&marker,4);}
  bind(&vb,stride,4);draw_vertices(h,3,0);assert(!dev.error&&submits==1);
  for(unsigned v=0;v<3;v++){expect(v,0,n,(uint32_t*)(vb.data+v*stride)+2);expect(v,1,m,(uint32_t*)(vb.data+v*stride)+2+n);}
  /* R16/R32 indexed, nonzero index offset/start and negative base. */
  for(unsigned sz=2;sz<=4;sz+=2){
   uint32_t indices32[5]={99,99,3,2,1};uint16_t indices16[5]={99,99,3,2,1};
   struct geo_typed_resource ib={2,5*sz,1,1,sz==2?(BYTE*)indices16:(BYTE*)indices32};
   set_indices(h,(D3D10DDI_HRESOURCE){&ib},sz==2?DXGI_FORMAT_R16_UINT:DXGI_FORMAT_R32_UINT,sz);
   submits=0;draw_indices(h,3,1,-1);assert(!dev.error&&submits==1);
   for(unsigned v=0;v<3;v++){expect(v,0,n,(uint32_t*)(vb.data+(2-v)*stride)+2);expect(v,1,m,(uint32_t*)(vb.data+(2-v)*stride)+2+n);}
   ib.bytes=4*sz;submits=0;draw_indices(h,3,1,-1);assert(dev.error==E_INVALIDARG&&!submits);dev.error=0;
   ib.bytes=5*sz;draw_indices(h,3,1,-4);assert(dev.error==E_INVALIDARG);dev.error=0;
   unsigned before=maps;draw_indices(h,3,UINT32_MAX,0);assert(dev.error==E_INVALIDARG&&maps==before);dev.error=0;
   set_indices(h,(D3D10DDI_HRESOURCE){0},DXGI_FORMAT_R32_UINT,0);assert(ib.refs==1);
  }
  dev.vertex_offset=UINT32_MAX;unsigned before=maps;FLOAT tmp[4];assert(read_vertex(&dev,1,0,tmp)==E_INVALIDARG&&maps==before);dev.vertex_offset=4;
  bind(&vb,65537,0);assert(dev.error==E_INVALIDARG&&dev.vertex_offset==4);dev.error=0;
  set_vertices(h,1,1,(D3D10DDI_HRESOURCE[]){{&vb}},(UINT[]){0},(UINT[]){0});assert(dev.error==E_NOTIMPL);dev.error=0;
  bind(NULL,0,0);free(vb.data);
 }

 /* Every possible byte in every RGBA channel: exact4 read at buffer end,
  * stride0, direct rational-RNE oracle (not a reciprocal implementation). */
 install_formats(DXGI_FORMAT_R8G8B8A8_UNORM,0,0);
 BYTE*rgba=malloc(4);struct geo_typed_resource ub={1,4,1,1,rgba};bind(&ub,0,0);
 for(unsigned x=0;x<256;x++){
  rgba[0]=x;rgba[1]=255-x;rgba[2]=(x+73)&255;rgba[3]=(x*17)&255;
  submits=0;draw_vertices(h,3,0);assert(!dev.error&&submits==1);
  for(unsigned v=0;v<3;v++)for(unsigned ch=0;ch<4;ch++)assert(captured[v][ch]==unorm_expected[rgba[ch]]);
 }
 assert(unorm_expected[0]==0&&unorm_expected[255]==0x3f800000);
 for(unsigned x=1;x<256;x++)assert(unorm_expected[x]>unorm_expected[x-1]);
 ub.bytes=3;submits=0;draw_vertices(h,3,0);assert(dev.error==E_INVALIDARG&&!submits);dev.error=0;ub.bytes=4;
 bind(NULL,0,0);free(rgba);
 /* Mixed FLOAT/UNORM in either register, offsets/padding, R16/R32 indexing. */
 for(unsigned order=0;order<2;order++){
  install_formats(order?DXGI_FORMAT_R32G32B32A32_FLOAT:DXGI_FORMAT_R8G8B8A8_UNORM,order?DXGI_FORMAT_R8G8B8A8_UNORM:DXGI_FORMAT_R32G32B32A32_FLOAT,4);
  BYTE*data=calloc(3,32);struct geo_typed_resource vb={1,92,1,1,data};
  const uint32_t raw[4]={0x80000000,0x7f812345,0x7f800000,0xff800000};
  for(unsigned v=0;v<3;v++){
   memcpy(data+v*32+8+(order?0:4),raw,16);
   BYTE*q=data+v*32+8+(order?16:0);q[0]=v;q[1]=255-v;q[2]=64+v;q[3]=128+v;
  }
  bind(&vb,32,4);draw_vertices(h,3,0);assert(!dev.error&&submits==1);
  for(unsigned v=0;v<3;v++){
   expect(v,order?0:1,4,raw);BYTE*q=data+v*32+8+(order?16:0);
   for(unsigned ch=0;ch<4;ch++)assert(captured[v][(order?1:0)*4+ch]==unorm_expected[q[ch]]);
  }
  for(unsigned sz=2;sz<=4;sz+=2){
   uint16_t ix16[3]={2,1,0};uint32_t ix32[3]={2,1,0};struct geo_typed_resource ib={2,3*sz,1,1,sz==2?(BYTE*)ix16:(BYTE*)ix32};
   set_indices(h,(D3D10DDI_HRESOURCE){&ib},sz==2?DXGI_FORMAT_R16_UINT:DXGI_FORMAT_R32_UINT,0);
   submits=0;draw_indices(h,3,0,0);assert(!dev.error&&submits==1);
   for(unsigned v=0;v<3;v++){
    expect(v,order?0:1,4,raw);BYTE*q=data+(2-v)*32+8+(order?16:0);
    for(unsigned ch=0;ch<4;ch++)assert(captured[v][(order?1:0)*4+ch]==unorm_expected[q[ch]]);
   }
   set_indices(h,(D3D10DDI_HRESOURCE){0},DXGI_FORMAT_R32_UINT,0);assert(ib.refs==1);
  }
  vb.bytes=91;submits=0;draw_vertices(h,3,0);assert(dev.error==E_INVALIDARG&&!submits);dev.error=0;
  bind(NULL,0,0);free(data);
 }
 /* Retained layout profile: no other formats/slots/registers/instancing. */
 D3D10DDIARG_INPUT_ELEMENT_DESC e[2]={{0,0,DXGI_FORMAT_R32_FLOAT,0,0,0},{0,0,DXGI_FORMAT_R32_FLOAT,0,0,1}};D3D10DDIARG_CREATEELEMENTLAYOUT p={e,1};
 assert(!layout_desc(&p));e[0].Format=DXGI_FORMAT_R32_UINT;assert(layout_desc(&p)==E_NOTIMPL);e[0].Format=DXGI_FORMAT_R32_FLOAT;
 e[0].InputSlot=1;assert(layout_desc(&p)==E_NOTIMPL);e[0].InputSlot=0;e[0].InputRegister=2;assert(layout_desc(&p)==E_NOTIMPL);e[0].InputRegister=0;
 e[0].InstanceDataStepRate=1;assert(layout_desc(&p)==E_NOTIMPL);e[0].InstanceDataStepRate=0;e[0].InputSlotClass=1;assert(layout_desc(&p)==E_NOTIMPL);e[0].InputSlotClass=0;
 e[0].AlignedByteOffset=65532;assert(!layout_desc(&p));e[0].AlignedByteOffset=UINT32_MAX;assert(layout_desc(&p)==E_NOTIMPL);e[0].AlignedByteOffset=0;
 p.NumElements=2;e[1].InputRegister=0;assert(layout_desc(&p)==E_NOTIMPL);p.NumElements=3;assert(layout_desc(&p)==E_NOTIMPL);
 set_layout(h,(D3D10DDI_HELEMENTLAYOUT){0});destroy_layout(h,lhandle);assert(!dev.layouts);
 puts("PASS actual IA FLOAT1..4 + RGBA8_UNORM all256/channel RNE, stride0, mixed pairs/indexed/OOB; GPU submit mocked");return 0;
}
'''
c=out/'production-ia.c';c.write_text(code);exe=out/'ia-float-inputs'
cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c),'-o',str(exe)]
subprocess.run(cmd,check=True);subprocess.run([str(exe)],check=True)
(out/'result.json').write_text(json.dumps({'actual_IA_callbacks_and_resource_bounds':True,'all_float_widths_and_16_pairs':True,'zero_and_overlapping_stride':True,'R16_R32_indexed_and_negative_base':True,'bounds_overflow_map_unmap_failure_and_bitpatterns':True,'asan_ubsan':True,'typed_draw_sha256':hashlib.sha256((d/'umd/typed_draw.c').read_bytes()).hexdigest(),'RGBA8_UNORM_all256_each_channel_exact_RNE':True,'mixed_FLOAT_UNORM_both_registers_indexed':True,'RGBA8_shader_sample_capability_retained':True,'GPU_submit_mocked':True,'hardware_verified':False},indent=2)+'\n')
