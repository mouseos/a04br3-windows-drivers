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
#include <stdlib.h>
#include "clear_engine.h"
typedef uint32_t UINT;typedef uintptr_t ULONG_PTR;typedef unsigned char UCHAR;
typedef int32_t NTSTATUS;typedef void *HANDLE;typedef size_t SIZE_T;
#define MAXULONG UINT32_MAX
#define STATUS_INSUFFICIENT_RESOURCES ((int32_t)0xc000009a)
#define STATUS_INVALID_DEVICE_STATE GEO_NOT_READY
#define NonPagedPool 0
static int allocation_fail;
static void *allocate(size_t n){return allocation_fail?NULL:malloc(n);}
#define ExAllocatePoolWithTag(pool,n,tag) allocate(n)
#define ExFreePoolWithTag(p,tag) free(p)
#define InterlockedOr(p,n) (*(p)|=(n))
#define InterlockedExchange(p,n) (*(p)=(n))
#define DXGK_INTERRUPT_DMA_COMPLETED 1
static int irql;
static unsigned notifications,dpc_queued;
typedef struct {UINT InterruptType;struct {UINT SubmissionFenceId,NodeOrdinal,EngineOrdinal;}DmaCompleted;} DXGKARGCB_NOTIFY_INTERRUPT_DATA;
struct geo_present_batch {UINT count,index;struct geo_copy_region regions[1];};
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
#define DISPATCH_LEVEL 2
#define KeGetCurrentIrql() irql
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER GEO_INVALID
#define STATUS_NOT_SUPPORTED GEO_UNSUPPORTED
#define STATUS_DEVICE_NOT_READY GEO_NOT_READY
#define STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER ((int32_t)0xc01e0001)
#define DXGK_PRESENT_SOURCE_INDEX 1
#define DXGK_PRESENT_DESTINATION_INDEX 2
#define RtlZeroMemory(p,n) memset(p,0,n)
struct geo_kmd_adapter {struct geo_engine engine;int (*synchronize)(void*,int(*)(void*),void*);void*sync_context;struct {UINT generation;struct geo_present_batch*copy;}pending[GEO_MAX_JOBS];
 int notifications_ready,owner_irql,service_pending,dpc_pending;void*dxgk_device;
 void(*notify_interrupt)(void*,DXGKARGCB_NOTIFY_INTERRUPT_DATA*);void(*queue_dpc)(void*);};
struct geo_kmd_context {struct geo_kmd_adapter*adapter;};
static struct geo_allocation *GeoResolveReadAllocation(struct geo_kmd_context*c,HANDLE h){(void)c;return h;}
static struct geo_allocation *GeoResolveWriteAllocation(struct geo_kmd_context*c,HANDLE h){(void)c;return h;}
static int GeoSessionEnter(struct geo_kmd_adapter*a,int n){(void)a;(void)n;return 1;}
static void GeoSessionLeave(struct geo_kmd_adapter*a,int n){(void)a;(void)n;}
static void GeoCollectPendingPayloads(struct geo_kmd_adapter*a){for(UINT i=0;i<GEO_MAX_JOBS;i++)if(!a->engine.jobs[i].state){free(a->pending[i].copy);a->pending[i].copy=NULL;}}
static void GeoNotifyReadyPreemption(struct geo_kmd_adapter*a){(void)a;}
static NTSTATUS fail_copy_render(struct geo_kmd_adapter*a,NTSTATUS n){a->engine.poisoned=1;return n;}
static NTSTATUS GeoWriteTicket(void*p,const struct geo_dma_ticket*t){memcpy(p,t,sizeof(*t));return 0;}
struct copy_begin {struct geo_kmd_context *context;struct geo_allocation *destination,*source;HANDLE dh,sh;struct geo_copy_region region;struct geo_dma_ticket ticket;NTSTATUS status;};
'''
begin = s[s.index('static BOOLEAN begin_copy('):s.index('/* A failed Render')].replace('BOOLEAN', 'int')
complete = s[s.index('NTSTATUS GeoCompleteFromInterrupt('):s.index('void GeoClearDpc(')]
suffix = r'''
static int sync(void*a,int(*fn)(void*),void*p){(void)a;return fn(p);}
static int32_t copy(void*c,uint64_t s,uint64_t d,uint64_t*t){(void)c;(void)s;(void)d;(void)t;assert(0);return GEO_INVALID;}
static int32_t region(void*c,uint64_t s,uint64_t d,const struct geo_copy_region*r,uint64_t*t){(void)c;(void)s;(void)d;(void)r;(void)t;assert(0);return GEO_INVALID;}
static int32_t submit(void*c,uint64_t token,uint32_t fence,uint32_t generation){(void)c;assert(token&&fence&&generation);return 0;}
static void notify(void*p,DXGKARGCB_NOTIFY_INTERRUPT_DATA*d){(void)p;assert(d->DmaCompleted.SubmissionFenceId==1);notifications++;}
static void dpc(void*p){(void)p;dpc_queued++;}
int main(void){
 struct geo_kmd_adapter a={0};struct geo_kmd_context c={&a};struct geo_allocation primary={0},shadow={0};
 a.synchronize=sync;a.engine.owner.prepare_copy=copy;a.engine.owner.prepare_copy_region=region;a.engine.owner.submit=submit;
 a.notifications_ready=1;a.owner_irql=3;a.notify_interrupt=notify;a.queue_dpc=dpc;
 primary.surface=1;shadow.surface=2;primary.primary=1;primary.bound=shadow.bound=1;
 primary.logical_width=shadow.logical_width=800;primary.logical_height=shadow.logical_height=1280;
 primary.desc.usage=shadow.desc.usage=GEO_USAGE_STAGING;
 primary.desc.format=shadow.desc.format=GEO_FORMAT_X8R8G8B8;
 DXGK_ALLOCATIONLIST list[3]={{0},{&shadow,0},{&primary,1}};
 struct geo_dma_ticket dma={0},priv={0},claimed;PATCH patch[2];
 RECT dirty[33];for(UINT i=0;i<33;i++)dirty[i]=(RECT){(int)i,0,(int)i+1,100};
 DXGKARG_PRESENT p={0};p.pAllocationList=list;p.Flags.Value=1;
 p.SrcRect=p.DstRect=(RECT){0,0,800,1280};p.SubRectCnt=33;p.pDstSubRects=dirty;
 p.pDmaBuffer=&dma;p.DmaSize=sizeof(dma);p.pDmaBufferPrivateData=&priv;p.DmaBufferPrivateDataSize=sizeof(priv);
 p.pPatchLocationListOut=patch;p.PatchLocationListOutSize=2;
 DXGKARG_PRESENT initial=p;
 dirty[32].right=801;assert(GeoPresent(&c,&p)==GEO_INVALID);assert(!primary.references&&!shadow.references);
 dirty[32].right=33;p.DmaSize=0;assert(GeoPresent(&c,&p)==STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER);assert(!primary.references&&!shadow.references&&!p.MultipassOffset);
 p=initial;allocation_fail=1;assert(GeoPresent(&c,&p)==STATUS_INSUFFICIENT_RESOURCES);allocation_fail=0;assert(!primary.references&&!shadow.references);
 assert(GeoPresent(&c,&p)==0&&p.MultipassOffset==33);
 assert(!memcmp(&dma,&priv,sizeof(dma))&&patch[0].AllocationIndex==2&&patch[1].AllocationIndex==1);
 struct geo_job*j=&a.engine.jobs[dma.slot];struct geo_present_batch*batch=a.pending[dma.slot].copy;
 assert(batch&&batch->count==33&&j->state==4&&primary.references==1&&shadow.references==1);
 memset(dirty,0,sizeof(dirty)); /* Deep copy, no borrowed RECT lifetime. */
 assert(!geo_submit_captured(&a.engine,&dma,1));
 irql=3;
 for(UINT i=0;i<33;i++){
  assert(geo_submitted_prepare_ready(&a.engine));assert(!geo_claim_submitted_prepare(&a.engine,&claimed));
  assert(claimed.slot==dma.slot&&j->copy_region.source_x==i);
  assert(!geo_finish_submitted_prepare(&a.engine,&claimed,0,100+i));
  if(i==10)assert(!geo_request_preempt(&a.engine,7));
  assert(!GeoCompleteFromInterrupt(&a,100+i,dma.generation,0,GEO_COMPLETION_GPU_DONE|GEO_COMPLETION_CPU_VISIBLE));
  if(i<32){assert(dpc_queued==i+1&&!a.dpc_pending);assert(j->state==GEO_JOB_COPY_CONTINUE&&primary.references==1&&shadow.references==1&&!notifications&&!a.engine.completed_ever);}
 }
 assert(notifications==1&&!j->state&&!primary.references&&!shadow.references&&a.engine.last_completed==1);
 uint32_t pf,last;assert(!geo_take_preemption(&a.engine,&pf,&last)&&pf==7&&last==1);
 irql=0;GeoCollectPendingPayloads(&a);assert(!a.pending[dma.slot].copy);
 /* Reverse direction and nonzero resume offset still use one job. */
 for(UINT i=0;i<33;i++)dirty[i]=(RECT){(int)i,0,(int)i+1,100};
 list[1].hDeviceSpecificAllocation=&primary;list[2].hDeviceSpecificAllocation=&shadow;
 p=initial;p.MultipassOffset=31;assert(!GeoPresent(&c,&p)&&p.MultipassOffset==33);
 j=&a.engine.jobs[dma.slot];batch=a.pending[dma.slot].copy;assert(batch->count==2&&j->allocation==&shadow&&j->vertex==&primary);
 assert(!geo_submit_captured(&a.engine,&dma,2));assert(!geo_claim_submitted_prepare(&a.engine,&claimed));assert(!geo_finish_submitted_prepare(&a.engine,&claimed,0,200));
 irql=3;assert(GeoCompleteFromInterrupt(&a,200,dma.generation,0,GEO_COMPLETION_GPU_DONE)!=0);
 assert(a.engine.poisoned&&j->state==3&&batch->index==0&&notifications==1&&primary.references==1&&shadow.references==1);
 irql=0;GeoCollectPendingPayloads(&a);assert(a.pending[dma.slot].copy==batch); /* No proof, no free. */
 /* Simulated owner drain invokes the real software proof-retirement path. */
 primary.next=&shadow;a.engine.allocations=&primary;
 assert(!geo_retire_after_owner_drain(&a.engine));GeoCollectPendingPayloads(&a);assert(!primary.references&&!shadow.references);
 struct geo_copy_region r;RECT src={20,30,120,230},dst={100,200,200,400},sub={110,210,130,250};
 assert(present_region(&src,&dst,&sub,&shadow,&primary,&r)&&r.source_x==30&&r.source_y==40);
 src.right++;assert(!present_region(&src,&dst,&sub,&shadow,&primary,&r));
 /* OS-preclipped copies may cross each outer destination edge. */
 src=(RECT){0,0,800,1280};
 dst=(RECT){-10,0,790,1280};sub=(RECT){0,0,790,1280};
 assert(present_region(&src,&dst,&sub,&shadow,&primary,&r)&&r.source_x==10&&r.destination_x==0&&r.width==790);
 dst=(RECT){10,0,810,1280};sub=(RECT){10,0,800,1280};
 assert(present_region(&src,&dst,&sub,&shadow,&primary,&r)&&r.source_x==0&&r.destination_x==10&&r.width==790);
 dst=(RECT){0,-10,800,1270};sub=(RECT){0,0,800,1270};
 assert(present_region(&src,&dst,&sub,&shadow,&primary,&r)&&r.source_y==10&&r.height==1270);
 dst=(RECT){0,10,800,1290};sub=(RECT){0,10,800,1280};
 assert(present_region(&src,&dst,&sub,&shadow,&primary,&r)&&r.source_y==0&&r.destination_y==10&&r.height==1270);
 /* Reject the same requests when the actual destination is not clipped. */
 for(UINT edge=0;edge<4;edge++){
  RECT unclipped[4]={{-10,0,790,1280},{10,0,810,1280},{0,-10,800,1270},{0,10,800,1290}};
  dst=unclipped[edge];sub=dst;assert(!present_region(&src,&dst,&sub,&shadow,&primary,&r));
 }

 src=(RECT){INT32_MIN,0,INT32_MAX,1280};assert(!present_region(&src,&dst,&sub,&shadow,&primary,&r));
 p=initial;p.MultipassOffset=33;assert(GeoPresent(&c,&p)==GEO_INVALID);
 p=initial;list[2].WriteOperation=0;assert(GeoPresent(&c,&p)==GEO_UNSUPPORTED);list[2].WriteOperation=1;
 shadow.desc.format=GEO_FORMAT_A8R8G8B8;assert(GeoPresent(&c,&p)==GEO_UNSUPPORTED);
 return 0;
}
'''
source = out / 'gdi-present.c'
source.write_text(prefix + begin + body + complete + suffix)
command = ['cc', '-std=c11', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
           '-I' + str(d), str(source), str(d / 'clear_engine.c'), str(d / 'clear_protocol.c'), '-o', str(out / 'gdi-present')]
subprocess.run(command, check=True)
subprocess.run([str(out / 'gdi-present')], check=True)
print('GDI_PRESENT_ACTUAL_BODY_AND_ENGINE_PASS; WDK/native/GPU untested')

# The actual C-linkage shared-primary callback needs no C++ constructs.
u = (d.parent / 'wddm-display/unified_display.cxx').read_text()
u = u[u.index('extern "C" NTSTATUS GeoDisplaySharedPrimaryLayout('):u.index('extern "C" NTSTATUS GeoDisplayValidatePostAddress(')].replace('extern "C" ', '')
primary_prefix = r"""
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint32_t UINT;typedef uint64_t ULONGLONG;typedef int32_t NTSTATUS;
#define STATUS_SUCCESS 0
#define STATUS_NOT_SUPPORTED ((int32_t)0xc00000bb)
#define NT_SUCCESS(x) ((x)>=0)
#define MAXULONG UINT32_MAX
#define MAXULONGLONG UINT64_MAX
#define D3DDDIFMT_A8R8G8B8 21
#define D3DDDIFMT_X8R8G8B8 22
#define RtlZeroMemory(p,n) memset(p,0,n)
typedef struct {UINT VidPnSourceId,Format,Width,Height;} D3DKMDT_SHAREDPRIMARYSURFACEDATA;
typedef struct {UINT Width,Height,Pitch,ColorFormat;struct {int64_t QuadPart;}PhysicAddress;} DXGK_DISPLAY_INFORMATION;
struct geo_post_primary_layout_v1 {UINT size,version,source_id,width,height,pitch,format;};
static NTSTATUS GeoDisplayPostInfo(void*h,DXGK_DISPLAY_INFORMATION*out){*out=*(DXGK_DISPLAY_INFORMATION*)h;return 0;}
"""
primary_suffix = r"""
int main(void){
 DXGK_DISPLAY_INFORMATION info={800,1280,3200,22,{0x90000000}};
 D3DKMDT_SHAREDPRIMARYSURFACEDATA p={0,21,800,1280};struct geo_post_primary_layout_v1 out;
 assert(!GeoDisplaySharedPrimaryLayout(&info,&p,&out)&&out.format==21&&out.pitch==3200);
 p.Format=22;assert(!GeoDisplaySharedPrimaryLayout(&info,&p,&out)&&out.format==22);
 p.Format=24;assert(GeoDisplaySharedPrimaryLayout(&info,&p,&out)==STATUS_NOT_SUPPORTED);
 p.Format=22;p.Width=799;assert(GeoDisplaySharedPrimaryLayout(&info,&p,&out)==STATUS_NOT_SUPPORTED);
 p.Width=800;info.Pitch=3196;assert(GeoDisplaySharedPrimaryLayout(&info,&p,&out)==STATUS_NOT_SUPPORTED);
 return 0;
}
"""
source = out / 'primary.c'
source.write_text(primary_prefix + u + primary_suffix)
subprocess.run(['cc', '-std=c11', '-g', '-fsanitize=address,undefined', str(source), '-o', str(out / 'primary')], check=True)
subprocess.run([str(out / 'primary')], check=True)
print('GDI_SHARED_PRIMARY_ACTUAL_CALLBACK_A8_X8_PASS; WDK/native/GPU untested')
