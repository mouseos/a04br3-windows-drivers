#!/usr/bin/env python3
"""Production RT selection/retention and role/viewport contracts; OS calls mocked."""
import argparse,pathlib,subprocess,os
root=pathlib.Path(__file__).resolve().parents[1];b=root/'drivers/gpu/mt8167/wddm-render';n=root/'drivers/gpu/mt8167/vendor_m1_11/render_owner'
a=argparse.ArgumentParser();a.add_argument('--output',type=pathlib.Path,required=True);out=a.parse_args().output.resolve();out.mkdir(parents=True,exist_ok=True)
source=(n/'native_owner_factory.c').read_text()
def section(start,end):return source[source.index(start):source.index(end,source.index(start))]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "clear_engine.h"
#include "viewport_profile.h"
#include "native_d3d_viewport.h"
#include "native_clear_surface.h"
#include "native_texture2d_state.h"
typedef int32_t NTSTATUS;typedef int BOOLEAN;
#define TRUE 1
#define FALSE 0
#define STATUS_SUCCESS GEO_OK
#define STATUS_INVALID_PARAMETER GEO_INVALID
#define STATUS_INVALID_DEVICE_STATE GEO_POISONED
#define STATUS_DEVICE_BUSY GEO_BUSY
#define NT_SUCCESS(x) ((x)>=0)
#define E_OWNER_FAILED 3
#define E_JOB_NONE 0
#define E_JOB_PREPARED 1
#define E_JOB_SUBMITTED 2
#define RtlZeroMemory(p,n) memset(p,0,n)
struct a04_native_mdl_binding {int id;};
struct a04_native_owner_allocation {void *native;struct a04_native_mdl_binding *binding,*vertex;BOOLEAN binding_borrowed;};
struct e_native_resource {struct e_native_resource *next;uint64_t id;struct geo_surface_desc desc;struct a04_native_mdl_binding *linear;uint64_t linear_offset;};
struct e_native_owner {struct e_native_resource *resources;struct a04_native_owner_allocation *allocation;uint64_t surface,aperture_offset,vertex,vertex_aperture_offset,job;struct geo_surface_desc description,vertex_description;unsigned job_state,state;};
static NTSTATUS boundary,stop_result,retire_result,build_result;
static unsigned waits,stops,retires,builds,releases;
static NTSTATUS enter(struct e_native_owner *c,int teardown){(void)c;(void)teardown;return 0;}
static NTSTATUS leave(struct e_native_owner *c,NTSTATUS s){(void)c;return s;}
static NTSTATUS wait_boundary_locked(struct e_native_owner *c){(void)c;waits++;return boundary;}
static NTSTATUS e_native_stop_locked(struct e_native_owner *c){(void)c;stops++;return stop_result;}
static NTSTATUS retire_from365(struct e_native_owner *c,unsigned origin,unsigned x){(void)c;(void)origin;(void)x;retires++;assert(stops>=retires);return retire_result;}
static NTSTATUS e_native_build_graph_locked(struct e_native_owner *c){assert(c->allocation->binding_borrowed);builds++;return build_result;}
static NTSTATUS a04_native_mdl_release_cold(struct a04_native_mdl_binding **p){assert(retires&&stops);assert(*p);releases++;*p=0;return 0;}
'''
# Admit through the actual typed resource predicate (WDK descriptor types mocked).
code+=r'''
typedef uint32_t UINT;typedef int32_t HRESULT;
#define E_NOTIMPL GEO_UNSUPPORTED
#define E_INVALIDARG GEO_INVALID
#define S_OK 0
#define D3D10_DDI_RESOURCE_MISC_SHARED 2u
#define D3D10_DDI_BIND_RENDER_TARGET 32u
#define D3D10_DDI_BIND_SHADER_RESOURCE 8u
#define D3D10_DDI_BIND_VERTEX_BUFFER 1u
#define D3D10_DDI_BIND_INDEX_BUFFER 2u
#define D3D10_DDI_BIND_CONSTANT_BUFFER 4u
#define D3D10_DDI_BIND_PRESENT 0x1000u
#define D3D10DDIRESOURCE_TEXTURE2D 3u
#define D3D10DDIRESOURCE_BUFFER 1u
#define D3D10_DDI_USAGE_DEFAULT 0u
#define D3D10_DDI_USAGE_IMMUTABLE 1u
#define D3D10_DDI_USAGE_DYNAMIC 2u
#define D3D10_DDI_USAGE_STAGING 3u
#define D3D10_DDI_CPU_ACCESS_WRITE 0x10000u
#define D3D10_DDI_CPU_ACCESS_MASK 0x30000u
#define DXGI_FORMAT_UNKNOWN 0u
#define DXGI_FORMAT_B8G8R8A8_UNORM 87u
#define DXGI_FORMAT_R8G8B8A8_UNORM 28u
typedef struct {UINT TexelWidth,TexelHeight,TexelDepth,PhysicalWidth,PhysicalHeight,PhysicalDepth;} D3D10DDI_MIPINFO;
struct primary {UINT VidPnSourceId;struct {UINT Width,Height;} ModeDesc;};
typedef struct {D3D10DDI_MIPINFO *pMipInfoList;UINT MipLevels,ArraySize,MiscFlags,ByteStride,DecoderBufferType,ResourceDimension,Usage,MapFlags,BindFlags,Format;struct primary *pPrimaryDesc;struct {UINT Count,Quality;} SampleDesc;void *pInitialDataUP;} D3D11DDIARG_CREATERESOURCE;
'''
typed=(b/'umd/typed_resource.c').read_text();code+=typed[typed.index('static HRESULT desc_valid('):typed.index('static SIZE_T APIENTRY resource_size(')]
code+=section('static struct e_native_resource *resource(','static int32_t create(')
code+=section('static int32_t unbind(','static int32_t destroy(')
code+=section('static NTSTATUS select_target_locked(','static int32_t prepare(')
runtime=(n/'native_owner_factory_runtime.c').read_text();start=runtime.index('NTSTATUS e_native_build_graph_locked(');start=runtime.index('    for(struct e_native_resource *linear',start);end=runtime.index('    return STATUS_SUCCESS;',start)
code+=r'''
static unsigned graph_maps;static struct a04_native_mdl_binding *selected,*vertex_selected;
static NTSTATUS a04_vendor_native_attach_linear_cold(void *native,struct a04_native_mdl_binding *binding,uint64_t offset){(void)native;(void)offset;assert(binding!=selected&&binding!=vertex_selected);graph_maps++;return 0;}
static NTSTATUS map_other_resident_resources(struct e_native_owner *c){struct a04_native_owner_allocation *a=c->allocation;NTSTATUS status;
'''+runtime[start:end]+'''return 0;}
'''
code+=r'''
static int32_t created(void *c,const struct geo_surface_desc *d,uint64_t *id){(void)d;*id=++*(uint64_t*)c;return 0;}
int main(void){
 D3D10DDI_MIPINFO mip={800,A04_NATIVE_FRAME_HEIGHT,1,800,A04_NATIVE_FRAME_HEIGHT,1};D3D11DDIARG_CREATERESOURCE create={0};create.pMipInfoList=&mip;create.MipLevels=create.ArraySize=create.SampleDesc.Count=1;create.ResourceDimension=3;create.BindFlags=40;create.Format=87;
 assert(!desc_valid(&create));create.MiscFlags=2;assert(!desc_valid(&create));create.Format=28;assert(desc_valid(&create)==E_NOTIMPL);create.Format=87;
 create.pInitialDataUP=&mip;assert(desc_valid(&create)==E_NOTIMPL);create.pInitialDataUP=0;mip.TexelHeight=mip.PhysicalHeight=A04_NATIVE_FRAME_HEIGHT-1;assert(desc_valid(&create)==E_NOTIMPL);mip.TexelHeight=mip.PhysicalHeight=A04_NATIVE_FRAME_HEIGHT;
 struct geo_surface_desc rt={sizeof(rt),1,800,A04_NATIVE_FRAME_HEIGHT,3200,GEO_FORMAT_A8R8G8B8,GEO_USAGE_RT_TEXTURE2D,0};
 struct geo_shared_surface wire={0};wire.base=(struct geo_allocation_data){sizeof(wire),3,800,A04_NATIVE_FRAME_HEIGHT,rt};wire.magic=GEO_SHARED_MAGIC;wire.bind=GEO_SHARED_RT|GEO_SHARED_SRV;wire.format=GEO_SHARED_BGRA;wire.dimension=3;wire.misc=GEO_SHARED_MISC;
 assert(geo_shared_surface_valid(&wire));wire.base.logical_width=799;assert(!geo_shared_surface_valid(&wire));
 struct geo_surface_desc bad=rt;bad.version=2;assert(!geo_desc_valid(&bad));bad=rt;bad.format=GEO_FORMAT_A8B8G8R8;assert(!geo_desc_valid(&bad));
 struct a04_m111_clear_surface pbe;assert(!a04_m111_build_clear_surface(&pbe,0x100000,800,A04_NATIVE_FRAME_HEIGHT,800));
 assert(((pbe.csr[0]>>32)&3u)==0u&&((pbe.csr[0]>>34)&0xffffu)==399u); /* LINEAR PBE: same physical stride as SMP. */
 struct a04_texture2d_storage storage={sizeof(storage),1,800,A04_NATIVE_FRAME_HEIGHT,3200,3200*A04_NATIVE_FRAME_HEIGHT,A04_TEXTURE2D_BGRA8,1,1,1,0,0x100000};struct a04_texture2d_state sample;
 assert(!a04_m111_texture2d_state(&sample,&storage));assert((sample.image[2]&0x7fffu)==799u); /* SMP stride is pixels-minus-one. */
 struct geo_engine engine={0};uint64_t ids=0;engine.owner.context=&ids;engine.owner.create=created;
 struct geo_allocation a={0},x={0},v={0};struct geo_surface_desc vd={sizeof(vd),1,64,1,64,GEO_FORMAT_VERTEXDATA,GEO_USAGE_VERTEX_BUFFER,0};struct geo_dma_ticket ticket;
 assert(!geo_create(&engine,&a,&rt)&&!geo_create(&engine,&x,&rt)&&!geo_create(&engine,&v,&vd));assert(!a.cpu_only&&!x.cpu_only);
 assert(geo_prepare_texture_begin(&engine,&a,&v,&a,&ticket)==GEO_INVALID);
 assert(!geo_prepare_texture_begin(&engine,&a,&v,&x,&ticket));assert(a.references==1&&x.references==1&&v.references==1);
 assert(geo_destroy(&engine,&x)==GEO_BUSY); /* Sampled RT lifetime retained by actual pending job. */
 struct a04_native_mdl_binding ma={1},mb={2};struct a04_native_owner_allocation allocation={0};
 struct e_native_resource rb={0,2,rt,&mb,0x400000},ra={&rb,1,rt,&ma,0};struct e_native_owner c={0};c.resources=&ra;c.allocation=&allocation;
 assert(!select_target_locked(&c,1)&&c.surface==1&&allocation.binding==&ma&&allocation.binding_borrowed);assert(stops==1&&retires==1&&builds==1);
 assert(!select_target_locked(&c,1)&&stops==1&&builds==1); /* Same completed target has no ownership churn. */
 boundary=GEO_BUSY;assert(select_target_locked(&c,2)==GEO_BUSY&&allocation.binding==&ma&&stops==1);boundary=0;
 stop_result=GEO_POISONED;assert(select_target_locked(&c,2)==GEO_POISONED&&allocation.binding==&ma&&retires==1);stop_result=0;c.state=1;
 retire_result=GEO_POISONED;assert(select_target_locked(&c,2)==GEO_POISONED&&allocation.binding==&ma);retire_result=0;c.state=1;
 assert(!select_target_locked(&c,2)&&allocation.binding==&mb&&ra.linear==&ma&&c.aperture_offset==0x400000);assert(!releases); /* No MDL clone/release on RT->SRV transition. */
 selected=&mb;vertex_selected=NULL;assert(!map_other_resident_resources(&c)&&graph_maps==1); /* Actual graph loop never double-registers selected target MDL. */
 assert(!unbind(&c,2)&&!allocation.binding&&!allocation.binding_borrowed&&!c.surface&&!rb.linear&&ra.linear==&ma&&releases==1);
 assert(!select_target_locked(&c,1));build_result=GEO_POISONED;rb.linear=&mb;assert(select_target_locked(&c,2)==GEO_POISONED&&c.state==E_OWNER_FAILED&&allocation.binding==&mb&&rb.linear==&mb); /* Failed graph pins new owner. */
 float floats[6]={10,20,320,240,0,1};uint32_t f[6],native[6];memcpy(f,floats,sizeof(f));assert(geo_viewport_profile_valid(f));assert(!a04_m111_build_d3d_viewport(native,f));
 float nx=170,ny=140,nw=160,nh=-120;uint32_t expected[4];memcpy(expected,(float[]){nx,nw,ny,nh},sizeof(expected));assert(!memcmp(native,expected,sizeof(expected)));
 floats[2]=320.5f;memcpy(f,floats,sizeof(f));assert(!geo_viewport_profile_valid(f)&&a04_m111_build_d3d_viewport(native,f)==GEO_UNSUPPORTED);
 puts("PASS production role/lease/RT selection failure retention and shared viewport emission; mocked OS/native graph operations; no GPU completion simulated");return 0;
}
'''
fixture=out/'rt-srv-production.c';fixture.write_text(code)
for unified in [0,1]:
 exe=out/f'rt-srv-{unified}';cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer',f'-DA04_GE8300_UNIFIED_DISPLAY356={unified}','-I'+str(b),'-I'+str(n),str(fixture),str(b/'clear_engine.c'),str(b/'clear_protocol.c'),str(n/'native_d3d_viewport.c'),str(n/'native_clear_surface.c'),str(n/'native_texture2d_state.c'),'-o',str(exe)]
 subprocess.run(cmd,check=True,env={**os.environ,'TMPDIR':str(out)});subprocess.run([str(exe)],check=True)
