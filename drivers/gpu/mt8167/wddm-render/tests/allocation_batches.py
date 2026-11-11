#!/usr/bin/env python3
"""Run the actual four DDI callbacks and private native transactions on host.

Windows types/IRQ/session/pool/display are modeled, not a WDK ABI or hardware
qualification. Source functions are extracted unchanged, never reimplemented.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import subprocess

ap = argparse.ArgumentParser()
ap.add_argument('--output', type=Path, required=True)
a = ap.parse_args()
w = Path(__file__).resolve().parents[1]
repo = w.parents[3]
out = a.output.resolve()
# Repo worktrees and project logs are both below TAB-A04-BR3.
project = repo.parent.parent.resolve()
if not out.is_relative_to(project) or out.exists():
    raise ValueError('New project-root output required')
out.mkdir(parents=True)
n = w.parent / 'vendor_m1_11/render_owner/native_owner_factory.c'
ns, ks, obs, eng = (p.read_text() for p in (n, w/'kmd_clear.c', w/'kmd_objects.c', w/'clear_engine.c'))

def section(s, begin, end):
    return s[s.index(begin):s.index(end, s.index(begin))]

parts = [
    section(ns, 'static NTSTATUS enter(', 'static BOOLEAN close_admission('),
    section(ns, 'static struct e_native_resource *resource(', '/* Capture scheduler/native progress'),
    section(ns, 'static int32_t destroy(', 'static NTSTATUS select_target_locked('),
    section(eng, 'int32_t geo_create(', '/* Pending Render packets'),
    section(eng, 'int32_t geo_destroy(', 'static struct geo_job *job('),
    section(ks, 'struct geo_allocation_prepared {', 'static NTSTATUS GeoBuildPagingBufferImpl('),
    section(obs, 'struct allocation_open_batch {', 'struct geo_allocation *GeoResolveReadAllocation('),
    section(ks, 'NTSTATUS APIENTRY GeoCreateAllocation(', 'NTSTATUS APIENTRY GeoBuildPagingBuffer('),
]
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "clear_engine.h"
typedef void *HANDLE;typedef int32_t NTSTATUS;typedef uint32_t UINT;typedef int BOOLEAN;
#define TRUE 1
#define FALSE 0
#define APIENTRY
#define STATUS_SUCCESS 0
#define STATUS_NOT_SUPPORTED ((NTSTATUS)0xc00000bb)
#define STATUS_INVALID_PARAMETER ((NTSTATUS)0xc000000d)
#define STATUS_DEVICE_NOT_READY ((NTSTATUS)0xc00000a3)
#define STATUS_INSUFFICIENT_RESOURCES ((NTSTATUS)0xc000009a)
#define STATUS_DEVICE_BUSY ((NTSTATUS)0x80000011)
#define STATUS_INVALID_DEVICE_STATE ((NTSTATUS)0xc0000184)
#define STATUS_INTEGER_OVERFLOW ((NTSTATUS)0xc0000095)
#define NT_SUCCESS(x) ((x)>=0)
#define PASSIVE_LEVEL 0
#define NonPagedPool 0
#define NonPagedPoolNx 1
#define Executive 0
#define KernelMode 0
#define PowerDeviceD0 1
#define E_OWNER_ACTIVE 1
#define E_OWNER_FAILED 3
#define E_FACTORY_TAG 0x664e4741u
#define PAGE_SIZE 4096u
#define ROUND_TO_PAGES(x) (((x)+4095u)&~4095u)
#define D3DDDI_ALLOCATIONPRIORITY_NORMAL 0x78000000u
#define DXGK_HANDLE_ALLOCATION 0
#define RtlZeroMemory(p,n) memset((p),0,(n))
#define RtlCopyMemory memcpy
#include "allocation_batch.h"
static int irq,deny_rundown,session_depth,live=1,sync_fail,retire_calls,end_maps;
static int pool_live,pool_calls,fail_after=-1;
static int KeGetCurrentIrql(void){return irq;}
static void *ExAllocatePoolWithTag(int type,size_t size,unsigned tag){
 (void)type;(void)tag;assert(irq==0);pool_calls++;
 if(fail_after==0)return NULL;if(fail_after>0)fail_after--;
 void *p=malloc(size);if(p)pool_live++;return p;
}
static void ExFreePoolWithTag(void *p,unsigned tag){(void)tag;assert(irq==0&&p);pool_live--;free(p);}
static int ExAcquireRundownProtection(int *p){if(deny_rundown)return 0;(*p)++;return 1;}
static void ExReleaseRundownProtection(int *p){assert(*p>0);(*p)--;}
static int KeWaitForSingleObject(int *p,int a,int b,int c,void *d){(void)a;(void)b;(void)c;(void)d;assert(!*p);*p=1;return 0;}
static void KeReleaseMutex(int *p,int b){(void)b;assert(*p==1);*p=0;}
struct e_native_resource {struct e_native_resource *next;uint64_t id;struct geo_surface_desc desc;void *linear;uint64_t linear_offset;};
struct fake_native_allocation {void *vertex,*binding;};
struct e_native_owner {struct e_native_resource *resources;uint64_t next_id,surface,vertex;int mutex,operations,closing,leased,state,power;struct fake_native_allocation *allocation;};
typedef struct {void *hAllocation,*pPrivateDriverData;UINT PrivateDriverDataSize;uint64_t Alignment,Size,PitchAlignedSize;
 union {UINT Value;struct {UINT SegmentId0:5;};} PreferredSegment;
 UINT SupportedReadSegmentSet,SupportedWriteSegmentSet,EvictionSegmentSet;
 struct {UINT Value;} HintedBank;UINT MaximumRenamingListLength;void *pAllocationUsageHint;
 union {UINT Value;struct {UINT CpuVisible:1,Cached:1,SynchronousPaging:1;};} Flags;UINT AllocationPriority;
} DXGK_ALLOCATIONINFO;
typedef struct {UINT NumAllocations;DXGK_ALLOCATIONINFO *pAllocationInfo;void *pPrivateDriverData;UINT PrivateDriverDataSize;HANDLE hResource;
 union {UINT Value;struct {UINT Resource:1;};} Flags;} DXGKARG_CREATEALLOCATION;
typedef struct {UINT NumAllocations;HANDLE *pAllocationList;HANDLE hResource;
 union {UINT Value;struct {UINT DestroyResource:1;};} Flags;} DXGKARG_DESTROYALLOCATION;
typedef struct {HANDLE hAllocation,hDeviceSpecificAllocation;void *pPrivateDriverData;UINT PrivateDriverDataSize;} DXGK_OPENALLOCATIONINFO;
typedef struct {UINT NumAllocations;DXGK_OPENALLOCATIONINFO *pOpenAllocation;void *pPrivateDriverData;UINT PrivateDriverSize;
 union {UINT Value;struct {UINT Create:1,ReadOnly:1;};} Flags;UINT SubresourceIndex;} DXGKARG_OPENALLOCATION;
typedef struct {UINT NumAllocations;HANDLE *pOpenHandleList;} DXGKARG_CLOSEALLOCATION;
typedef struct {HANDLE hObject;UINT Type;} DXGKARGCB_GETHANDLEDATA;
struct geo_kmd_shared_resource {struct geo_kmd_shared_resource *next;struct geo_allocation *allocation;};
struct geo_kmd_adapter {struct geo_engine engine;struct geo_kmd_shared_resource *shared_resources;void *display;
 int aperture_ready;struct {uint64_t commit_limit;} aperture;
 void *(*get_handle_data)(DXGKARGCB_GETHANDLEDATA*);
 BOOLEAN (*synchronize)(void*,BOOLEAN(*)(void*),void*);void *sync_context;};
struct geo_open_allocation {struct geo_open_allocation *next;struct geo_allocation *allocation;UINT readonly;};
struct geo_kmd_device {struct geo_kmd_adapter *adapter;struct geo_open_allocation *opens;};
static int GeoSessionEnter(struct geo_kmd_adapter *a,int mode){(void)mode;if(!a||irq||!live)return 0;assert(!session_depth);session_depth=1;return 1;}
static void GeoSessionLeave(struct geo_kmd_adapter *a,int mode){(void)a;(void)mode;assert(session_depth==1);session_depth=0;}
static NTSTATUS GeoDisplay356RetireAllocation(struct geo_kmd_adapter *a,struct geo_allocation *x){(void)a;assert(irq==0);retire_calls++;x->display_refs=0;return 0;}
static void GeoEndVidMmMap(struct geo_kmd_adapter *a,struct geo_allocation *x){(void)a;(void)x;assert(irq==0);end_maps++;}
static BOOLEAN sync_irq(void *p,BOOLEAN(*fn)(void*),void *arg){(void)p;assert(irq==0&&session_depth);if(sync_fail)return 0;irq=2;int ok=fn(arg);irq=0;return ok;}
static void *handle_data(DXGKARGCB_GETHANDLEDATA *q){assert(q->Type==DXGK_HANDLE_ALLOCATION);return q->hObject;}
'''
suffix = r'''
static struct e_native_owner native;
static struct fake_native_allocation physical;
static struct geo_kmd_adapter adapter;
static struct geo_kmd_device device;
static struct geo_allocation_data data[3];
static DXGK_ALLOCATIONINFO infos[3],before[3];
static DXGKARG_CREATEALLOCATION make_create(unsigned count){
 DXGKARG_CREATEALLOCATION p={0};
 for(unsigned i=0;i<3;i++){
  data[i]=(struct geo_allocation_data){sizeof(data[i]),1,800,1280,{sizeof(struct geo_surface_desc),1,800,1280,3200,21,GEO_USAGE_STAGING,0}};
  memset(&infos[i],0xa5,sizeof(infos[i]));infos[i].pPrivateDriverData=&data[i];infos[i].PrivateDriverDataSize=sizeof(data[i]);
 }
 memcpy(before,infos,sizeof(infos));p.NumAllocations=count;p.pAllocationInfo=infos;return p;
}
static unsigned native_count(void){unsigned n=0;for(struct e_native_resource *r=native.resources;r;r=r->next)n++;return n;}
static void assert_clean(void){assert(!pool_live&&!adapter.engine.allocation_count&&!adapter.engine.allocations&&!native.resources&&!device.opens&&!session_depth&&!native.mutex&&!native.operations);}
static void setup(void){
 assert(pool_live==0);memset(&native,0,sizeof(native));memset(&physical,0,sizeof(physical));native.allocation=&physical;native.leased=1;native.state=E_OWNER_ACTIVE;native.power=PowerDeviceD0;
 memset(&adapter,0,sizeof(adapter));adapter.engine.owner=(struct geo_owner_ops){0};
 adapter.engine.owner.size=sizeof(struct geo_owner_ops);adapter.engine.owner.version=GEO_OWNER_OPS_VERSION;adapter.engine.owner.context=&native;
 adapter.engine.owner.create=create;adapter.engine.owner.destroy=destroy;
 adapter.aperture_ready=1;adapter.aperture.commit_limit=8*1024*1024;adapter.synchronize=sync_irq;adapter.get_handle_data=handle_data;
 memset(&device,0,sizeof(device));device.adapter=&adapter;
}
static void destroy_three(void){HANDLE handles[3]={infos[0].hAllocation,infos[1].hAllocation,infos[2].hAllocation};DXGKARG_DESTROYALLOCATION p={0};p.NumAllocations=3;p.pAllocationList=handles;assert(GeoDestroyAllocation(&adapter,&p)==0);}
int main(void){
 setup();DXGKARG_CREATEALLOCATION p=make_create(3);int calls=pool_calls;
 data[2].storage.reserved=1;assert(GeoCreateAllocation(&adapter,&p)!=0);assert(!memcmp(infos,before,sizeof(infos))&&native.next_id==0&&pool_calls==calls+1);assert_clean();
 /* Every pool failure in frontend + native prepare, including the last native node. */
 for(int f=0;f<7;f++){
  p=make_create(3);fail_after=f;assert(GeoCreateAllocation(&adapter,&p)!=0);fail_after=-1;
  assert(!memcmp(infos,before,sizeof(infos))&&native.next_id==0);assert_clean();
 }
 p=make_create(3);deny_rundown=1;assert(GeoCreateAllocation(&adapter,&p)!=0);deny_rundown=0;assert_clean();assert(!memcmp(infos,before,sizeof(infos)));
 native.closing=1;assert(GeoCreateAllocation(&adapter,&p)!=0);native.closing=0;assert_clean();
 native.leased=0;assert(GeoCreateAllocation(&adapter,&p)!=0);native.leased=1;assert_clean();
 native.state=E_OWNER_FAILED;assert(GeoCreateAllocation(&adapter,&p)!=0);native.state=E_OWNER_ACTIVE;assert_clean();
 native.next_id=UINT64_MAX-2;assert(GeoCreateAllocation(&adapter,&p)==STATUS_INTEGER_OVERFLOW);assert_clean();native.next_id=0;
 adapter.engine.allocation_count=UINT32_MAX-1;assert(GeoCreateAllocation(&adapter,&p)==STATUS_INTEGER_OVERFLOW);adapter.engine.allocation_count=0;assert_clean();
 p.NumAllocations=0;assert(GeoCreateAllocation(&adapter,&p)!=0);p.NumAllocations=257;assert(GeoCreateAllocation(&adapter,&p)!=0);
 p.NumAllocations=3;irq=2;assert(GeoCreateAllocation(&adapter,&p)!=0);irq=0;live=0;assert(GeoCreateAllocation(&adapter,&p)!=0);live=1;assert_clean();
 assert(GeoCreateAllocation(&adapter,&p)==0);assert(adapter.engine.allocation_count==3&&native_count()==3&&native.next_id==3);
 struct geo_allocation *xs[3];for(unsigned i=0;i<3;i++){xs[i]=infos[i].hAllocation;assert(xs[i]&&xs[i]->surface==i+1&&xs[i]->cpu_only&&infos[i].PitchAlignedSize==0);}
 int held=pool_live;
 DXGK_OPENALLOCATIONINFO oi[3]={0},obefore[3];for(unsigned i=0;i<3;i++){oi[i].hAllocation=xs[i];oi[i].hDeviceSpecificAllocation=(void*)0x1234;}memcpy(obefore,oi,sizeof(oi));
 DXGKARG_OPENALLOCATION op={0};op.NumAllocations=3;op.pOpenAllocation=oi;
 oi[2].hAllocation=(void*)0xdead;assert(GeoOpenAllocation(&device,&op)!=0);oi[2].hAllocation=xs[2];assert(pool_live==held&&!xs[0]->open_count&&!device.opens);
 oi[2].hAllocation=xs[0];assert(GeoOpenAllocation(&device,&op)!=0);oi[2].hAllocation=xs[2];assert(pool_live==held&&!xs[0]->open_count);
 for(int f=0;f<4;f++){fail_after=f;assert(GeoOpenAllocation(&device,&op)!=0);fail_after=-1;assert(pool_live==held&&!xs[0]->open_count&&!device.opens&&!memcmp(oi,obefore,sizeof(oi)));}
 xs[2]->open_count=UINT32_MAX;assert(GeoOpenAllocation(&device,&op)==STATUS_INTEGER_OVERFLOW);xs[2]->open_count=0;assert(pool_live==held&&!xs[0]->open_count);
 xs[2]->poisoned=1;assert(GeoOpenAllocation(&device,&op)!=0);xs[2]->poisoned=0;assert(pool_live==held&&!xs[0]->open_count);
 sync_fail=1;assert(GeoOpenAllocation(&device,&op)!=0);sync_fail=0;assert(pool_live==held&&!xs[0]->open_count);
 op.Flags.Value=4;assert(GeoOpenAllocation(&device,&op)!=0);op.Flags.Value=2;
 assert(GeoOpenAllocation(&device,&op)==0);for(unsigned i=0;i<3;i++){assert(xs[i]->open_count==1);assert(((struct geo_open_allocation*)oi[i].hDeviceSpecificAllocation)->readonly==1);}
 HANDLE hs[3]={oi[0].hDeviceSpecificAllocation,oi[1].hDeviceSpecificAllocation,oi[2].hDeviceSpecificAllocation};
 DXGKARG_CLOSEALLOCATION cl={3,hs};held=pool_live;
 hs[2]=hs[0];assert(GeoCloseAllocation(&device,&cl)!=0);hs[2]=oi[2].hDeviceSpecificAllocation;assert(pool_live==held&&xs[0]->open_count==1);
 hs[2]=(void*)0xdead;assert(GeoCloseAllocation(&device,&cl)!=0);hs[2]=oi[2].hDeviceSpecificAllocation;
 struct geo_kmd_device other={&adapter,NULL};assert(GeoCloseAllocation(&other,&cl)!=0);
 adapter.engine.jobs[0].state=1;adapter.engine.jobs[0].texture_open_handle=hs[2];assert(GeoCloseAllocation(&device,&cl)==STATUS_DEVICE_BUSY);assert(pool_live==held&&xs[0]->open_count==1);
 adapter.engine.jobs[0].state=4; /* preserve existing captured-but-unsubmitted close semantics */
 xs[2]->open_count=0;assert(GeoCloseAllocation(&device,&cl)!=0);xs[2]->open_count=1;assert(xs[0]->open_count==1);
 fail_after=0;assert(GeoCloseAllocation(&device,&cl)!=0);fail_after=-1;assert(pool_live==held&&xs[0]->open_count==1);
 sync_fail=1;assert(GeoCloseAllocation(&device,&cl)!=0);sync_fail=0;assert(xs[0]->open_count==1);
 assert(GeoCloseAllocation(&device,&cl)==0);for(unsigned i=0;i<3;i++)assert(!xs[i]->open_count);assert(!device.opens);
 memset(&adapter.engine.jobs[0],0,sizeof(adapter.engine.jobs[0]));
 /* Distinct open handles on the SAME allocation: aggregate reference validation. */
 op.NumAllocations=1;op.Flags.Value=0;assert(GeoOpenAllocation(&device,&op)==0);HANDLE aopen=oi[0].hDeviceSpecificAllocation;
 assert(GeoOpenAllocation(&device,&op)==0);HANDLE two[]={aopen,oi[0].hDeviceSpecificAllocation};DXGKARG_CLOSEALLOCATION same={2,two};
 xs[0]->open_count=1;assert(GeoCloseAllocation(&device,&same)!=0&&xs[0]->open_count==1);xs[0]->open_count=2;assert(GeoCloseAllocation(&device,&same)==0&&!device.opens);
 HANDLE allocs[3]={xs[0],xs[1],xs[2]};DXGKARG_DESTROYALLOCATION dp={0};dp.NumAllocations=3;dp.pAllocationList=allocs;held=pool_live;
 allocs[2]=allocs[0];assert(GeoDestroyAllocation(&adapter,&dp)!=0);allocs[2]=xs[2];assert(pool_live==held&&native_count()==3);
 allocs[2]=(void*)0xdead;assert(GeoDestroyAllocation(&adapter,&dp)!=0);allocs[2]=xs[2];
 xs[2]->references=1;assert(GeoDestroyAllocation(&adapter,&dp)==STATUS_DEVICE_BUSY);xs[2]->references=0;
 xs[2]->open_count=1;assert(GeoDestroyAllocation(&adapter,&dp)==STATUS_DEVICE_BUSY);xs[2]->open_count=0;
 xs[2]->bound=1;assert(GeoDestroyAllocation(&adapter,&dp)==STATUS_DEVICE_BUSY);xs[2]->bound=0;
 xs[2]->display_refs=1;assert(GeoDestroyAllocation(&adapter,&dp)==STATUS_DEVICE_BUSY);xs[2]->display_refs=0;assert(retire_calls==0);
 xs[2]->poisoned=1;assert(GeoDestroyAllocation(&adapter,&dp)!=0);xs[2]->poisoned=0;
 adapter.engine.poisoned=1;assert(GeoDestroyAllocation(&adapter,&dp)!=0);adapter.engine.poisoned=0;
 sync_fail=1;assert(GeoDestroyAllocation(&adapter,&dp)!=0);sync_fail=0;
 for(int f=0;f<2;f++){fail_after=f;assert(GeoDestroyAllocation(&adapter,&dp)!=0);fail_after=-1;assert(pool_live==held&&native_count()==3&&adapter.engine.allocation_count==3);}
 struct e_native_resource *last=resource(&native,xs[2]->surface);last->linear=(void*)1;assert(GeoDestroyAllocation(&adapter,&dp)==STATUS_DEVICE_BUSY);last->linear=NULL;
 native.surface=xs[2]->surface;physical.binding=(void*)1;assert(GeoDestroyAllocation(&adapter,&dp)==STATUS_DEVICE_BUSY);physical.binding=NULL;native.surface=0;
 deny_rundown=1;assert(GeoDestroyAllocation(&adapter,&dp)!=0);deny_rundown=0;
 native.closing=1;assert(GeoDestroyAllocation(&adapter,&dp)!=0);native.closing=0;
 assert(pool_live==held&&native_count()==3&&adapter.engine.allocation_count==3);
 uint64_t tokens[]={xs[0]->surface,xs[1]->surface,UINT64_MAX};assert(GeoNativeDestroyAllocationBatch(&adapter.engine.owner,3,tokens)!=0);assert(native_count()==3);
 tokens[2]=tokens[0];assert(GeoNativeDestroyAllocationBatch(&adapter.engine.owner,3,tokens)!=0);assert(native_count()==3);
 destroy_three();assert_clean();
 /* Single primary still uses original creation + display-retirement path. */
 p=make_create(1);struct geo_allocation_data_v2 primary={0};primary.base=data[0];primary.base.size=sizeof(primary);primary.base.version=2;primary.flags=GEO_ALLOCATION_PRIMARY;
 infos[0].PrivateDriverDataSize=sizeof(primary);infos[0].pPrivateDriverData=&primary;adapter.display=(void*)1;
 assert(GeoCreateAllocation(&adapter,&p)==0);struct geo_allocation *x=infos[0].hAllocation;assert(x->primary&&!x->cpu_only&&infos[0].PitchAlignedSize==infos[0].Size);
 x->display_refs=1;HANDLE one=x;DXGKARG_DESTROYALLOCATION single={0};single.NumAllocations=1;single.pAllocationList=&one;assert(GeoDestroyAllocation(&adapter,&single)==0&&retire_calls==1);assert_clean();
 /* Existing shared single resource lifecycle including allocation-only then resource-only. */
 p=make_create(1);struct geo_shared_surface sh={0};sh.base=data[0];sh.base.size=sizeof(sh);sh.base.version=3;sh.base.storage.usage=GEO_USAGE_RENDER_TARGET;
 sh.base.storage.width=800;sh.base.storage.height=600;sh.base.storage.pitch=3200;sh.base.logical_height=600;
 sh.magic=GEO_SHARED_MAGIC;sh.bind=GEO_SHARED_RT;sh.format=GEO_SHARED_BGRA;sh.dimension=3;sh.misc=GEO_SHARED_MISC;
 infos[0].pPrivateDriverData=&sh;infos[0].PrivateDriverDataSize=sizeof(sh);p.pPrivateDriverData=&sh;p.PrivateDriverDataSize=sizeof(sh);p.Flags.Resource=1;
 assert(GeoCreateAllocation(&adapter,&p)==0);assert(p.hResource&&adapter.shared_resources);
 /* The existing shared wire still denotes one allocation/resource. Reject a
  * multi request before publishing any additional native/frontend node. */
 DXGK_ALLOCATIONINFO sharedmany[2]={infos[0],infos[0]};DXGKARG_CREATEALLOCATION multi=p;
 multi.NumAllocations=2;multi.pAllocationInfo=sharedmany;int sharedheld=pool_live;
 assert(GeoCreateAllocation(&adapter,&multi)==STATUS_NOT_SUPPORTED);
 assert(pool_live==sharedheld&&native_count()==1&&adapter.engine.allocation_count==1&&multi.hResource==p.hResource);
 x=infos[0].hAllocation;op.NumAllocations=1;op.PrivateDriverSize=sizeof(sh);op.pPrivateDriverData=&sh;oi[0].hAllocation=x;oi[0].pPrivateDriverData=&sh;oi[0].PrivateDriverDataSize=sizeof(sh);
 assert(GeoOpenAllocation(&device,&op)==0);HANDLE shared_open=oi[0].hDeviceSpecificAllocation;DXGKARG_CLOSEALLOCATION sc={1,&shared_open};assert(GeoCloseAllocation(&device,&sc)==0);
 one=x;single.hResource=p.hResource;assert(GeoDestroyAllocation(&adapter,&single)==0);assert(adapter.shared_resources&&!adapter.shared_resources->allocation);
 single.NumAllocations=0;single.Flags.DestroyResource=1;assert(GeoDestroyAllocation(&adapter,&single)==0&&!adapter.shared_resources);assert_clean();
 /* Maximum accepted batch exercises the bound without kernel-stack arrays. */
 DXGK_ALLOCATIONINFO *many=calloc(GEO_ALLOCATION_BATCH_MAX,sizeof(*many));HANDLE *mh=calloc(GEO_ALLOCATION_BATCH_MAX,sizeof(*mh));assert(many&&mh);
 p=make_create(1);p.NumAllocations=GEO_ALLOCATION_BATCH_MAX;p.pAllocationInfo=many;
 for(unsigned i=0;i<GEO_ALLOCATION_BATCH_MAX;i++){many[i].pPrivateDriverData=&data[0];many[i].PrivateDriverDataSize=sizeof(data[0]);}
 assert(GeoCreateAllocation(&adapter,&p)==0&&native_count()==GEO_ALLOCATION_BATCH_MAX);
 for(unsigned i=0;i<GEO_ALLOCATION_BATCH_MAX;i++)mh[i]=many[i].hAllocation;
 DXGK_OPENALLOCATIONINFO *mo=calloc(GEO_ALLOCATION_BATCH_MAX,sizeof(*mo));HANDLE *mc=calloc(GEO_ALLOCATION_BATCH_MAX,sizeof(*mc));assert(mo&&mc);
 for(unsigned i=0;i<GEO_ALLOCATION_BATCH_MAX;i++)mo[i].hAllocation=mh[i];
 op=(DXGKARG_OPENALLOCATION){0};op.NumAllocations=GEO_ALLOCATION_BATCH_MAX;op.pOpenAllocation=mo;assert(GeoOpenAllocation(&device,&op)==0);
 for(unsigned i=0;i<GEO_ALLOCATION_BATCH_MAX;i++)mc[i]=mo[i].hDeviceSpecificAllocation;
 cl.NumAllocations=GEO_ALLOCATION_BATCH_MAX;cl.pOpenHandleList=mc;assert(GeoCloseAllocation(&device,&cl)==0);free(mc);free(mo);
 dp.NumAllocations=GEO_ALLOCATION_BATCH_MAX;dp.pAllocationList=mh;assert(GeoDestroyAllocation(&adapter,&dp)==0);free(mh);free(many);assert_clean();
 puts("ALLOCATION_BATCH_CALLBACKS_PASS");return 0;
}
'''
fixture = out/'callbacks.c'
fixture.write_text(prefix + '\n'.join(parts) + suffix)
cmd = ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-multichar',
       '-Wno-misleading-indentation', '-fsanitize=address,undefined',
       '-fno-omit-frame-pointer', '-DA04_GE8300_UNIFIED_DISPLAY356=1', '-I'+str(w), str(fixture), str(w/'clear_protocol.c'),
       '-o', str(out/'callbacks')]
env = {**os.environ, 'TMPDIR': str(out)}
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
with (out/'host.log').open('w') as log:
    subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    subprocess.run([str(out/'callbacks')], env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
(out/'result.json').write_text(json.dumps({
    'actual_ddi_callbacks': ['GeoCreateAllocation', 'GeoDestroyAllocation', 'GeoOpenAllocation', 'GeoCloseAllocation'],
    'actual_native_helpers': True, 'asan_ubsan': True,
    'validation': ['late invalid input', 'duplicate handle/token', 'reference/ID/count overflow',
                   'all create/open pool-failure points', 'close/destroy preparation pool failure',
                   'native admission failure', 'native late busy', 'IRQ synchronization failure',
                   'cross-device close', 'aggregate close references', 'display-backed multi unchanged',
                   'single primary/display retirement', 'single shared allocation/resource/open/close',
                   '256-entry bound for all four DDIs', 'shared-wire multi rejection'],
    'source_sha256': {str(p.relative_to(repo)): hashlib.sha256(p.read_bytes()).hexdigest()
                      for p in (n, w/'kmd_clear.c', w/'kmd_objects.c', w/'allocation_batch.h', w/'clear_engine.c')},
    'not_validated': ['Windows WDK compile/link', 'hardware', 'real IRQ/rundown concurrency', 'active0 causality'],
}, indent=2)+'\n')
print('ALLOCATION_BATCH_CALLBACKS_PASS')
