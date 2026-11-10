#!/usr/bin/env python3
"""Actual callback/protocol/CPU-only engine source checks; no GPU/device access."""
import argparse,json,subprocess
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
d=Path(__file__).resolve().parents[1];repo=d.parents[4];out=a.output.resolve()
if not out.is_relative_to(repo.parent.parent.resolve()) or out.exists():raise ValueError('New task-root output required')
out.mkdir();s=(d/'kmd_objects.c').read_text();callback=s[s.index('NTSTATUS APIENTRY GeoGetStandardAllocationDriverData('):s.index('struct geo_debug_snapshot')]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "clear_engine.h"
typedef void *HANDLE;typedef int32_t NTSTATUS;typedef uint32_t UINT;
#define A04_GE8300_UNIFIED_DISPLAY356 0
#define D3DKMDT_STANDARDALLOCATION_SHAREDPRIMARYSURFACE 1
#define D3DDDIFMT_X8R8G8B8 22
#define APIENTRY
#define STATUS_SUCCESS 0
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xc00000a3)
#define D3DKMDT_STANDARDALLOCATION_SHADOWSURFACE 2
#define D3DKMDT_STANDARDALLOCATION_STAGINGSURFACE 3
#define D3DDDIFMT_A8R8G8B8 21
/* Fields mirror WDK8.1; the independent real-WDK compile checks actual ABI. */
typedef struct {UINT Width,Height,Format,Pitch;} D3DKMDT_SHADOWSURFACEDATA;
typedef struct {UINT Width,Height,Pitch;} D3DKMDT_STAGINGSURFACEDATA;
typedef struct {UINT StandardAllocationType;union {D3DKMDT_SHADOWSURFACEDATA*pCreateShadowSurfaceData;D3DKMDT_STAGINGSURFACEDATA*pCreateStagingSurfaceData;};void*pCreateSharedPrimarySurfaceData;void*pAllocationPrivateDriverData;UINT AllocationPrivateDriverDataSize;void*pResourcePrivateDriverData;UINT ResourcePrivateDriverDataSize;} DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA;
struct geo_kmd_adapter {int live;};
static NTSTATUS GeoDisplay356PrimaryLayout(void*a,void*b,void*c){return STATUS_NOT_SUPPORTED;}static unsigned enter,leave,owner_calls;
static int GeoSessionEnter(struct geo_kmd_adapter*a,int reason){enter++;return a&&a->live;}
static void GeoSessionLeave(struct geo_kmd_adapter*a,int reason){(void)a;(void)reason;leave++;}
#define RtlZeroMemory(p,n) memset((p),0,(n))
#define RtlCopyMemory memcpy
'''
suffix=r'''
static int32_t no_create(void*c,const struct geo_surface_desc*d,uint64_t*t){(void)c;(void)d;(void)t;owner_calls++;*t=1;return GEO_OK;}
static int32_t no_destroy(void*c,uint64_t t){(void)c;(void)t;owner_calls++;return GEO_OK;}
static int32_t native_bind(void*c,uint64_t t,void*m,uint32_t o,uint32_t b,uint64_t a){(void)c;(void)t;(void)m;(void)o;(void)b;(void)a;return GEO_OK;}
static int32_t native_unbind(void*c,uint64_t t){(void)c;(void)t;return GEO_OK;}
int main(void){
 struct geo_kmd_adapter adapter={1};D3DKMDT_STAGINGSURFACEDATA stage={800,600,0xdeadbeef};
 DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA p={0};struct {uint32_t before;struct geo_allocation_data data;uint32_t after;} guarded;
 unsigned char resource[16];struct geo_engine engine={0};struct geo_allocation alloc={0};struct geo_clear_packet packet={GEO_CLEAR_MAGIC,sizeof(packet),GEO_ABI_VERSION,0,0xff112233,0,{0,0}};struct geo_dma_ticket ticket;
 memset(resource,0xa5,sizeof(resource));memset(&guarded,0xcc,sizeof(guarded));
 p.StandardAllocationType=3;p.pCreateStagingSurfaceData=&stage;
 assert(GeoGetStandardAllocationDriverData(&adapter,&p)==0);assert(p.AllocationPrivateDriverDataSize==sizeof(guarded.data)&&p.ResourcePrivateDriverDataSize==0&&stage.Pitch==3200);
 p.pAllocationPrivateDriverData=&guarded.data;p.pResourcePrivateDriverData=resource;
 assert(GeoGetStandardAllocationDriverData(&adapter,&p)==0);assert(guarded.before==0xcccccccc&&guarded.after==0xcccccccc);
 for(unsigned i=0;i<sizeof(resource);i++)assert(resource[i]==0xa5);
 assert(guarded.data.logical_width==800&&guarded.data.logical_height==600);
 assert(guarded.data.storage.format==GEO_FORMAT_X8R8G8B8&&guarded.data.storage.usage==GEO_USAGE_STAGING&&geo_desc_valid(&guarded.data.storage));
 engine.owner.create=no_create;engine.owner.destroy=no_destroy;engine.owner.bind=native_bind;engine.owner.unbind=native_unbind;
 assert(geo_create(&engine,&alloc,&guarded.data.storage)==GEO_OK);assert(alloc.cpu_only&&alloc.surface==1&&engine.allocation_count==1&&owner_calls==1);
 assert(geo_bind(&engine,&alloc,&engine,0,3200*600,0)==GEO_OK&&owner_calls==1);
 assert(geo_prepare_begin(&engine,&alloc,&packet,&ticket)==GEO_INVALID&&owner_calls==1);
 assert(geo_unbind(&engine,&alloc)==GEO_OK);assert(geo_destroy(&engine,&alloc)==GEO_OK&&engine.allocation_count==0&&owner_calls==2);
 guarded.data.storage.usage=GEO_USAGE_RENDER_TARGET;assert(!geo_desc_valid(&guarded.data.storage));
 guarded.data.storage.format=GEO_FORMAT_A8R8G8B8;assert(geo_desc_valid(&guarded.data.storage));
 guarded.data.storage.usage=GEO_USAGE_STAGING;guarded.data.storage.format=22;guarded.data.storage.pitch=0xfffffffcu;assert(!geo_desc_valid(&guarded.data.storage));
 stage.Width=1024;stage.Height=1024;assert(GeoGetStandardAllocationDriverData(&adapter,&p)==0&&stage.Pitch==4096);
 stage.Width=1025;assert(GeoGetStandardAllocationDriverData(&adapter,&p)==STATUS_NOT_SUPPORTED);
 stage.Width=0;assert(GeoGetStandardAllocationDriverData(&adapter,&p)==STATUS_NOT_SUPPORTED);
 stage.Width=800;stage.Height=0;assert(GeoGetStandardAllocationDriverData(&adapter,&p)==STATUS_NOT_SUPPORTED);
 stage.Height=1025;assert(GeoGetStandardAllocationDriverData(&adapter,&p)==STATUS_NOT_SUPPORTED);
 stage.Height=600;stage.Width=0xffffffffu;assert(GeoGetStandardAllocationDriverData(&adapter,&p)==STATUS_NOT_SUPPORTED);
 p.pCreateStagingSurfaceData=0;assert(GeoGetStandardAllocationDriverData(&adapter,&p)==STATUS_INVALID_PARAMETER);
 p.StandardAllocationType=1;assert(GeoGetStandardAllocationDriverData(&adapter,&p)==STATUS_NOT_SUPPORTED);
 p.StandardAllocationType=4;assert(GeoGetStandardAllocationDriverData(&adapter,&p)==STATUS_NOT_SUPPORTED);
 {D3DKMDT_SHADOWSURFACEDATA shadow={800,600,21,0};p.StandardAllocationType=2;p.pCreateShadowSurfaceData=&shadow;assert(GeoGetStandardAllocationDriverData(&adapter,&p)==0&&shadow.Pitch==3200);assert(guarded.data.storage.format==21);shadow.Format=22;assert(GeoGetStandardAllocationDriverData(&adapter,&p)==0&&guarded.data.storage.format==22&&shadow.Pitch==3200);shadow.Format=24;assert(GeoGetStandardAllocationDriverData(&adapter,&p)==STATUS_NOT_SUPPORTED);}
 p.StandardAllocationType=3;p.pCreateStagingSurfaceData=&stage;stage.Width=800;adapter.live=0;assert(GeoGetStandardAllocationDriverData(&adapter,&p)==STATUS_DEVICE_NOT_READY);
 assert(GeoGetStandardAllocationDriverData(&adapter,0)==STATUS_INVALID_PARAMETER);
 return 0;
}
'''
c=out/'callback.c';c.write_text(prefix+callback+suffix)
cmd=['cc','-std=c11','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(d),str(c),str(d/'clear_protocol.c'),str(d/'clear_engine.c'),'-o',str(out/'standard-staging')]
subprocess.run(cmd,check=True);subprocess.run([str(out/'standard-staging')],check=True)
(out/'result.json').write_text(json.dumps({'actual_callback_executed':True,'actual_cpu_only_engine_executed':True,'two_call_size_fill_and_canaries':True,'x8_staging_and_shadow':True,'gpu_owner_create_and_destroy_calls':2,'bounds_and_legacy_shadow_checked':True,'asan_ubsan':True,'real_gpu_tested':False},indent=2)+'\n')
print('ACTUAL_STANDARD_STAGING_CALLBACK_AND_CPU_ONLY_ENGINE_PASS')
