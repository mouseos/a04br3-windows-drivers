#!/usr/bin/env python3
"""Execute actual Present and engine against host OS fixtures; no WDK/device claim."""
import argparse
import subprocess
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
d = Path(__file__).resolve().parents[1]
out = a.output.resolve()
if not out.is_relative_to(d.parents[5]) or out.exists():
    raise ValueError('Fresh project-local output required')
out.mkdir()
s = (d / 'kmd_clear.c').read_text()
body = s[s.index('static int present_region('):s.index('/* The two legal semantic lists')]
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "clear_engine.h"
typedef uint32_t UINT;typedef uintptr_t ULONG_PTR;typedef unsigned char UCHAR;
typedef int32_t NTSTATUS;typedef void *HANDLE;
typedef struct {int32_t left,top,right,bottom;} RECT;
typedef struct {HANDLE hDeviceSpecificAllocation;UINT WriteOperation;} DXGK_ALLOCATIONLIST;
typedef struct {UINT AllocationIndex,DriverId,Value,AllocationOffset,PatchOffset,SplitOffset;} PATCH;
typedef struct {void*pDmaBuffer,*pDmaBufferPrivateData;UINT DmaSize,DmaBufferPrivateDataSize;
 DXGK_ALLOCATIONLIST*pAllocationList;PATCH*pPatchLocationListOut;UINT PatchLocationListOutSize;
 UINT MultipassOffset,FlipInterval,DmaBufferSegmentId,SubRectCnt;const RECT*pDstSubRects;
 RECT SrcRect,DstRect;struct {UINT Value;} Flags;} DXGKARG_PRESENT;
#define TRUE 1
#define APIENTRY
#define PASSIVE_LEVEL 0
#define KeGetCurrentIrql() 0
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER GEO_INVALID
#define STATUS_NOT_SUPPORTED GEO_UNSUPPORTED
#define STATUS_DEVICE_NOT_READY GEO_NOT_READY
#define STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER ((int32_t)0xc01e0001)
#define DXGK_PRESENT_SOURCE_INDEX 1
#define DXGK_PRESENT_DESTINATION_INDEX 2
#define RtlZeroMemory(p,n) memset(p,0,n)
struct geo_kmd_adapter {struct geo_engine engine;int (*synchronize)(void*,int(*)(void*),void*);void*sync_context;struct {UINT generation;}pending[GEO_MAX_JOBS];};
struct geo_kmd_context {struct geo_kmd_adapter*adapter;};
static struct geo_allocation *GeoResolveReadAllocation(struct geo_kmd_context*c,HANDLE h){(void)c;return h;}
static struct geo_allocation *GeoResolveWriteAllocation(struct geo_kmd_context*c,HANDLE h){(void)c;return h;}
static int GeoSessionEnter(struct geo_kmd_adapter*a,int n){(void)a;(void)n;return 1;}
static void GeoSessionLeave(struct geo_kmd_adapter*a,int n){(void)a;(void)n;}
static void GeoCollectPendingPayloads(struct geo_kmd_adapter*a){(void)a;}
static NTSTATUS fail_copy_render(struct geo_kmd_adapter*a,NTSTATUS n){a->engine.poisoned=1;return n;}
static NTSTATUS GeoWriteTicket(void*p,const struct geo_dma_ticket*t){memcpy(p,t,sizeof(*t));return 0;}
struct copy_begin {struct geo_kmd_context *context;struct geo_allocation *destination,*source;HANDLE dh,sh;struct geo_copy_region region;struct geo_dma_ticket ticket;NTSTATUS status;};
'''
begin = s[s.index('static BOOLEAN begin_copy('):s.index('/* A failed Render')].replace('BOOLEAN', 'int')
suffix = r'''
static int sync(void*a,int(*fn)(void*),void*p){(void)a;return fn(p);}
static int32_t copy(void*c,uint64_t s,uint64_t d,uint64_t*t){(void)c;(void)s;(void)d;(void)t;assert(0);return GEO_INVALID;}
static int32_t region(void*c,uint64_t s,uint64_t d,const struct geo_copy_region*r,uint64_t*t){(void)c;(void)s;(void)d;(void)r;(void)t;assert(0);return GEO_INVALID;}
int main(void){
 struct geo_kmd_adapter a={0};struct geo_kmd_context c={&a};struct geo_allocation primary={0},shadow={0};
 a.synchronize=sync;a.engine.owner.prepare_copy=copy;a.engine.owner.prepare_copy_region=region;
 primary.surface=1;shadow.surface=2;primary.primary=1;
 primary.logical_width=shadow.logical_width=800;primary.logical_height=shadow.logical_height=1280;
 primary.desc.usage=shadow.desc.usage=GEO_USAGE_STAGING;
 primary.desc.format=shadow.desc.format=GEO_FORMAT_X8R8G8B8;
 DXGK_ALLOCATIONLIST list[3]={{0},{&shadow,0},{&primary,1}};
 struct geo_dma_ticket dma={0},priv={0};PATCH patch[2];
 RECT dirty[3]={{0,0,100,200},{780,1260,800,1280},{200,300,400,500}};
 DXGKARG_PRESENT p={0};p.pAllocationList=list;p.Flags.Value=1;
 p.SrcRect=p.DstRect=(RECT){0,0,800,1280};p.SubRectCnt=3;p.pDstSubRects=dirty;
 p.pDmaBuffer=&dma;p.DmaSize=sizeof(dma);p.pDmaBufferPrivateData=&priv;p.DmaBufferPrivateDataSize=sizeof(priv);
 p.pPatchLocationListOut=patch;p.PatchLocationListOutSize=2;
 DXGKARG_PRESENT initial=p;
 dirty[2].right=801;assert(GeoPresent(&c,&p)==GEO_INVALID);assert(!primary.references&&!shadow.references&&!p.MultipassOffset);
 dirty[2].right=400;p.DmaSize=0;assert(GeoPresent(&c,&p)==STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER);assert(!primary.references&&!shadow.references);
 for(UINT i=0;i<3;i++){
  p=initial;p.MultipassOffset=i;
  assert(GeoPresent(&c,&p)==(i==2?0:STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER));
  assert(p.MultipassOffset==i+1&&dma.magic==GEO_TICKET_MAGIC&&!memcmp(&dma,&priv,sizeof(dma)));
  struct geo_job*j=&a.engine.jobs[dma.slot];assert(j->state==4&&j->context==&c&&j->allocation==&primary&&j->vertex==&shadow);
  assert(j->copy_region.source_x==(UINT)dirty[i].left&&j->copy_region.destination_y==(UINT)dirty[i].top);
  assert(j->copy_region.width==(UINT)(dirty[i].right-dirty[i].left));
  assert(patch[0].AllocationIndex==2&&patch[1].AllocationIndex==1);
  assert(p.pDmaBuffer==(UCHAR*)&dma+sizeof(dma)&&p.pDmaBufferPrivateData==(UCHAR*)&priv+sizeof(priv));
 }
 assert(primary.references==3&&shadow.references==3);
 /* Reverse transfer retains the same actual allocations in opposite roles. */
 list[1].hDeviceSpecificAllocation=&primary;list[2].hDeviceSpecificAllocation=&shadow;
 p=initial;p.SubRectCnt=0;p.pDstSubRects=0;assert(GeoPresent(&c,&p)==0);
 assert(a.engine.jobs[dma.slot].allocation==&shadow&&a.engine.jobs[dma.slot].vertex==&primary);
 assert(primary.references==4&&shadow.references==4);
 /* Source/destination translation, no scaling and no integer overflow. */
 struct geo_copy_region r;RECT src={20,30,120,230},dst={100,200,200,400},sub={110,210,130,250};
 assert(present_region(&src,&dst,&sub,&shadow,&primary,&r));assert(r.source_x==30&&r.source_y==40&&r.width==20&&r.height==40);
 src.right++;assert(!present_region(&src,&dst,&sub,&shadow,&primary,&r));
 src=(RECT){INT32_MIN,0,INT32_MAX,1280};assert(!present_region(&src,&dst,&sub,&shadow,&primary,&r));
 p=initial;p.MultipassOffset=3;assert(GeoPresent(&c,&p)==GEO_INVALID);
 p=initial;list[2].WriteOperation=0;assert(GeoPresent(&c,&p)==GEO_UNSUPPORTED);list[2].WriteOperation=1;
 shadow.desc.format=GEO_FORMAT_A8R8G8B8;assert(GeoPresent(&c,&p)==GEO_UNSUPPORTED);
 assert(primary.references==4&&shadow.references==4);
 return 0;
}
'''
source = out / 'gdi-present.c'
source.write_text(prefix + begin + body + suffix)
command = ['cc', '-std=c11', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
           '-I' + str(d), str(source), str(d / 'clear_engine.c'), str(d / 'clear_protocol.c'), '-o', str(out / 'gdi-present')]
subprocess.run(command, check=True)
subprocess.run([str(out / 'gdi-present')], check=True)
print('GDI_PRESENT_ACTUAL_BODY_AND_ENGINE_PASS; WDK/native/GPU untested')
