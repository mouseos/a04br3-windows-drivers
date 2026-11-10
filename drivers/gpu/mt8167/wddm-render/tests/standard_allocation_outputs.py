#!/usr/bin/env python3
"""Execute the actual standard-allocation callback with output-size sentinels."""
import argparse
import json
import os
import resource
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
driver = Path(__file__).resolve().parents[1]
repo = driver.parents[4]
output = args.output.resolve()
if not output.is_relative_to(repo.parent.parent.resolve()) or output.exists():
    raise ValueError('New task-root output required')
output.mkdir()
source = (driver / 'kmd_objects.c').read_text()
callback = source[source.index('NTSTATUS APIENTRY GeoGetStandardAllocationDriverData('):
                  source.index('struct geo_debug_snapshot')]
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "clear_protocol.h"
typedef void *HANDLE;
typedef int32_t NTSTATUS;
typedef uint32_t UINT;
#define APIENTRY
#define A04_GE8300_UNIFIED_DISPLAY356 1
#define STATUS_SUCCESS 0
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xc00000a3)
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xc0000023)
#define D3DKMDT_STANDARDALLOCATION_SHAREDPRIMARYSURFACE 1
#define D3DKMDT_STANDARDALLOCATION_SHADOWSURFACE 2
#define D3DKMDT_STANDARDALLOCATION_STAGINGSURFACE 3
#define D3DDDIFMT_A8R8G8B8 21
typedef struct {UINT Width, Height, Format; struct {UINT Numerator, Denominator;} RefreshRate; UINT VidPnSourceId;} D3DKMDT_SHAREDPRIMARYSURFACEDATA;
typedef struct {UINT Width, Height, Format, Pitch;} D3DKMDT_SHADOWSURFACEDATA;
typedef struct {UINT Width, Height, Pitch;} D3DKMDT_STAGINGSURFACEDATA;
/* WDK8.1 d3dkmddi.h: both size members are outputs, not capacities. */
typedef struct {
 UINT StandardAllocationType;
 union {D3DKMDT_SHAREDPRIMARYSURFACEDATA *pCreateSharedPrimarySurfaceData;
        D3DKMDT_SHADOWSURFACEDATA *pCreateShadowSurfaceData;
        D3DKMDT_STAGINGSURFACEDATA *pCreateStagingSurfaceData;};
 void *pAllocationPrivateDriverData; UINT AllocationPrivateDriverDataSize;
 void *pResourcePrivateDriverData; UINT ResourcePrivateDriverDataSize;
} DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA;
struct geo_kmd_adapter {int live;};
static int GeoSessionEnter(struct geo_kmd_adapter *a, int reason) {(void)reason; return a && a->live;}
static void GeoSessionLeave(struct geo_kmd_adapter *a, int reason) {(void)a; (void)reason;}
/* Isolate the size/fill contract from hardware POST acquisition. */
static NTSTATUS GeoDisplay356PrimaryLayout(struct geo_kmd_adapter *a,
 const D3DKMDT_SHAREDPRIMARYSURFACEDATA *p, struct geo_allocation_data_v2 *out) {
 if (!a || !a->live || !p) return STATUS_DEVICE_NOT_READY;
 memset(out, 0, sizeof(*out)); out->base.size=sizeof(*out); out->base.version=2;
 out->base.logical_width=p->Width; out->base.logical_height=p->Height;
 out->base.storage=(struct geo_surface_desc){sizeof(struct geo_surface_desc),1,p->Width,p->Height,p->Width*4,p->Format,GEO_USAGE_STAGING,0};
 out->flags=GEO_ALLOCATION_PRIMARY; out->source_id=p->VidPnSourceId;
 return STATUS_SUCCESS;
}
#define RtlZeroMemory(p,n) memset((p),0,(n))
#define RtlCopyMemory memcpy
'''
suffix = r'''
int main(void) {
 struct geo_kmd_adapter adapter={1};
 D3DKMDT_SHAREDPRIMARYSURFACEDATA primary={800,1280,21,{60,1},0};
 D3DKMDT_SHADOWSURFACEDATA shadow={800,1280,21,0};
 D3DKMDT_STAGINGSURFACEDATA staging={800,1280,0};
 const UINT sentinels[]={0,1,UINT32_MAX};
 for (UINT type=1; type<=3; ++type) {
  const UINT size=type==1?sizeof(struct geo_allocation_data_v2):sizeof(struct geo_allocation_data);
  for (UINT i=0; i<sizeof(sentinels)/sizeof(sentinels[0]); ++i) {
   DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA p={0};
   unsigned char allocation[sizeof(struct geo_allocation_data_v2)+8], resource_data[8];
   p.StandardAllocationType=type;
   if(type==1)p.pCreateSharedPrimarySurfaceData=&primary;
   else if(type==2)p.pCreateShadowSurfaceData=&shadow;
   else p.pCreateStagingSurfaceData=&staging;
   for (UINT fill=0; fill<2; ++fill) {
    memset(allocation,0xa5,sizeof(allocation)); memset(resource_data,0x5a,sizeof(resource_data));
    p.AllocationPrivateDriverDataSize=sentinels[i]; p.ResourcePrivateDriverDataSize=sentinels[i];
    p.pAllocationPrivateDriverData=fill?allocation+4:NULL;
    p.pResourcePrivateDriverData=resource_data;
    shadow.Pitch=staging.Pitch=0xdeadbeef;
    assert(GeoGetStandardAllocationDriverData(&adapter,&p)==STATUS_SUCCESS);
    assert(p.AllocationPrivateDriverDataSize==size && p.ResourcePrivateDriverDataSize==0);
    for(UINT j=0;j<sizeof(resource_data);++j)assert(resource_data[j]==0x5a);
    if(type==2)assert(shadow.Pitch==3200);
    if(type==3)assert(staging.Pitch==3200);
    for(UINT j=0;j<sizeof(allocation);++j)
     if(!fill || j<4 || j>=4+size)assert(allocation[j]==0xa5);
    if(fill) {
     struct geo_allocation_data data;
     memcpy(&data,allocation+4,sizeof(data));
     assert(data.size==size && data.logical_width==800 && data.logical_height==1280);
     assert(data.storage.pitch==3200 && data.storage.format==(type==3?22u:21u));
     if(type==1) {
      struct geo_allocation_data_v2 extended;
      memcpy(&extended,allocation+4,sizeof(extended));
      assert(extended.base.version==2 && extended.flags==GEO_ALLOCATION_PRIMARY && extended.source_id==0);
     } else assert(data.version==1);
    }
   }
  }
 }
 return 0;
}
'''
fixture = output / 'callback.c'
fixture.write_text(prefix + callback + suffix)
binary = output / 'standard-allocation-outputs'
subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                '-DA04_GE8300_UNIFIED_DISPLAY356=1', '-I' + str(driver),
                str(fixture), str(driver / 'clear_protocol.c'), '-o', str(binary)],
               env={**os.environ, 'TMPDIR': str(output)}, check=True)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
subprocess.run([str(binary)], check=True)
(output / 'result.json').write_text(json.dumps({
    'actual_callback_executed': True, 'allocation_types': ['shared-primary', 'shadow', 'staging'],
    'output_size_sentinels': [0, 1, 4294967295], 'size_and_fill_calls': 18,
    'allocation_canaries_and_resource_buffer_preserved': True,
    'asan_ubsan': True, 'hardware_post_layout_stubbed': True, 'real_gpu_tested': False,
}, indent=2) + '\n')
print('STANDARD_ALLOCATION_OUTPUT_CONTRACT_PASS')
