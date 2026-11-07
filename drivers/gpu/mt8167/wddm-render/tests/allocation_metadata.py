#!/usr/bin/env python3
"""Execute actual CreateAllocation body and engine; host fixture is not WDK ABI evidence."""
import argparse,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args()
d=Path(__file__).resolve().parents[1];out=a.output.resolve()
if not out.is_relative_to(d.parents[5]) or out.exists():raise ValueError('New root-contained output required')
out.mkdir()
s=(d/'kmd_clear.c').read_text();body=s[s.index('static NTSTATUS GeoCreateAllocationImpl('):s.index('static NTSTATUS GeoDestroyAllocationImpl(')]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "clear_engine.h"
typedef void *HANDLE;typedef int32_t NTSTATUS;
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER ((int32_t)0xc000000d)
#define STATUS_NOT_SUPPORTED ((int32_t)0xc00000bb)
#define STATUS_INSUFFICIENT_RESOURCES ((int32_t)0xc000009a)
#define PASSIVE_LEVEL 0
#define KeGetCurrentIrql() 0
#define NonPagedPool 0
#define PAGE_SIZE 4096u
#define ROUND_TO_PAGES(n) (((n)+4095u)&~4095u)
#define RtlZeroMemory(p,n) memset(p,0,n)
#define RtlCopyMemory memcpy
#define ExAllocatePoolWithTag(pool,n,tag) calloc(1,n)
#define ExFreePoolWithTag(p,tag) free(p)
#define D3DDDI_ALLOCATIONPRIORITY_NORMAL 0x78000000u
typedef struct {void*pPrivateDriverData;uint32_t PrivateDriverDataSize;HANDLE hAllocation;size_t Alignment,Size,PitchAlignedSize;union {uint32_t Value;struct {uint32_t SegmentId0:5;};} PreferredSegment;uint32_t SupportedReadSegmentSet,SupportedWriteSegmentSet,EvictionSegmentSet;union {uint32_t Value;struct {uint32_t CpuVisible:1,PermanentSysMem:1,Cached:1,reserved:8,SynchronousPaging:1;};} Flags;uint32_t AllocationPriority;} DXGK_ALLOCATIONINFO;
typedef struct {uint32_t NumAllocations;DXGK_ALLOCATIONINFO*pAllocationInfo;uint32_t PrivateDriverDataSize;} DXGKARG_CREATEALLOCATION;
struct geo_kmd_adapter {int aperture_ready;struct {size_t commit_limit;} aperture;struct geo_engine engine;};
static unsigned creates,destroys;
static int32_t create(void*c,const struct geo_surface_desc*d,uint64_t*t){(void)c;(void)d;creates++;*t=1;return GEO_OK;}
static int32_t destroy(void*c,uint64_t t){(void)c;(void)t;destroys++;return GEO_OK;}
'''
suffix=r'''
int main(void){
 struct geo_kmd_adapter a={0};struct geo_allocation_data data={0};DXGK_ALLOCATIONINFO i;DXGKARG_CREATEALLOCATION p={1,&i,0};
 a.aperture_ready=1;a.aperture.commit_limit=4*1024*1024;a.engine.owner.create=create;a.engine.owner.destroy=destroy;
 for(unsigned n=0;n<3;n++){
  memset(&data,0,sizeof(data));data.size=sizeof(data);data.version=1;data.logical_width=800;data.logical_height=600;
  data.storage=(struct geo_surface_desc){0};data.storage.size=sizeof(data.storage);data.storage.version=GEO_ABI_VERSION;data.storage.width=800;data.storage.height=600;data.storage.pitch=3200;
  data.storage.format=n==0?GEO_FORMAT_X8R8G8B8:GEO_FORMAT_A8R8G8B8;data.storage.usage=n==2?GEO_USAGE_RENDER_TARGET:GEO_USAGE_STAGING;
  memset(&i,0xa5,sizeof(i));i.pPrivateDriverData=&data;i.PrivateDriverDataSize=sizeof(data);
  assert(GeoCreateAllocationImpl(&a,&p)==0);struct geo_allocation *al=i.hAllocation;
  assert(i.Size==1921024&&i.Alignment==4096&&i.Flags.Value==0x805&&i.EvictionSegmentSet==0);
  assert(i.SupportedReadSegmentSet==(n==2)&&i.SupportedWriteSegmentSet==(n==2)&&i.PreferredSegment.Value==(n==2));
  assert(i.PitchAlignedSize==(n==2?i.Size:0));assert(al->cpu_only==(n!=2));assert(creates==(n==2));
  assert(geo_destroy(&a.engine,al)==0);free(al);
 }
 assert(creates==1&&destroys==1&&a.engine.allocation_count==0);return 0;
}
'''
(out/'allocation.c').write_text(prefix+body+suffix)
subprocess.run(['cc','-std=c11','-fsanitize=address,undefined','-g','-I'+str(d),str(out/'allocation.c'),str(d/'clear_protocol.c'),str(d/'clear_engine.c'),'-o',str(out/'allocation')],check=True)
subprocess.run([str(out/'allocation')],check=True)
(out/'result.json').write_text(json.dumps({'actual_create_allocation_body':True,'actual_engine':True,'a8_x8_cpu_only_pitch_zero':True,'gpu_segment_pitch_unchanged':True,'permanent_sysmem_unchanged_zero':True,'asan_ubsan':True,'hardware_tested':False},indent=2)+'\n')
print('ACTUAL_CREATE_ALLOCATION_METADATA_PASS')
