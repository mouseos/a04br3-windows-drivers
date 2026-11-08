#!/usr/bin/env python3
"""Actual copy Render/fail-closed functions, injected post-reservation failures."""
import argparse,subprocess,json,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[2];out=a.output.resolve()
if not out.is_relative_to(r) or out.exists():raise ValueError('New checkout-local output required')
out.mkdir(parents=True);source=r/'drivers/gpu/mt8167/wddm-render/kmd_clear.c';s=source.read_text()
def extract(name):
 start=s.index('static ',s.index(name)-30);brace=s.index('{',s.index(name));i=brace+1;depth=1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[start:i]
functions='\n'.join(extract(x) for x in ['begin_copy(','poison_copy_render(','fail_copy_render(','GeoRenderCopyImpl('])
code=r'''#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef int NTSTATUS,BOOLEAN,LONG;typedef unsigned UINT;typedef unsigned char UCHAR;typedef void*HANDLE;
#define TRUE 1
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER -1
#define STATUS_DEVICE_NOT_READY -2
#define GEO_COPY_MAGIC 0x31504f47u
#define GEO_SESSION_FAILED 5
#define RtlZeroMemory(p,n) memset(p,0,n)
static LONG InterlockedExchange(LONG*p,LONG v){LONG old=*p;*p=v;return old;}
struct geo_kmd_context;
struct geo_allocation{int refs;};struct geo_dma_ticket{UINT magic,slot,generation;};
struct geo_job{struct geo_kmd_context*context;HANDLE open_handle,vertex_open_handle;};
struct session{LONG state;};
struct engine{int poisoned;struct geo_job jobs[1];struct{int prepare_copy;}owner;};
struct geo_kmd_adapter{struct engine engine;struct{UINT generation;}pending[1];struct session*session;BOOLEAN(*synchronize)(void*,BOOLEAN(*)(void*),void*);void*sync_context;};
struct geo_kmd_context{struct geo_kmd_adapter*adapter;};
struct allocation_list{HANDLE hDeviceSpecificAllocation;UINT WriteOperation,SegmentId;};
struct patch{UINT AllocationIndex,DriverId;};
typedef struct{UINT AllocationListSize,PatchLocationListOutSize;struct allocation_list*pAllocationList;struct patch*pPatchLocationListOut;void*pDmaBuffer,*pDmaBufferPrivateData;}DXGKARG_RENDER;
struct geo_copy_packet{UINT magic,size,version,destination_index,source_index,width,height,reserved;};
struct copy_begin{struct geo_kmd_context*context;struct geo_allocation*destination,*source;HANDLE dh,sh;struct geo_dma_ticket ticket;NTSTATUS status;};
static struct geo_allocation dst,src;static unsigned mode,writes,syncs;
static NTSTATUS geo_prepare_copy_begin(struct engine*e,struct geo_allocation*d,struct geo_allocation*s,struct geo_dma_ticket*t){(void)e;if(mode==1)return -3;d->refs++;s->refs++;*t=(struct geo_dma_ticket){1,0,1};return 0;}
static struct geo_allocation*GeoResolveWriteAllocation(struct geo_kmd_context*c,HANDLE h){(void)c;(void)h;return &dst;}
static struct geo_allocation*GeoResolveReadAllocation(struct geo_kmd_context*c,HANDLE h){(void)c;(void)h;return &src;}
static NTSTATUS prepare_pending(struct geo_kmd_adapter*a,struct geo_dma_ticket*t){(void)a;(void)t;return mode==2||mode==7?-4:0;}
static NTSTATUS GeoWriteTicket(void*p,struct geo_dma_ticket*t){(void)p;(void)t;writes++;return (mode==3&&writes==1)||(mode==4&&writes==2)?-1:0;}
static BOOLEAN sync_call(void*p,BOOLEAN(*f)(void*),void*x){(void)p;syncs++;if(mode==5&&syncs==1)return 0;if(mode==7&&syncs==2)return 0;BOOLEAN v=f(x);return mode==6&&syncs==1?0:v;}
'''+functions+r'''
int main(void){unsigned cases=0;for(mode=0;mode<9;mode++){
 struct session session={1};struct geo_kmd_adapter a={0};a.session=&session;a.engine.owner.prepare_copy=1;a.synchronize=sync_call;
 struct geo_kmd_context c={&a};struct allocation_list list[2]={{(void*)1,1,1},{(void*)2,0,1}};struct patch patches[2]={{0}};unsigned char dma[32],priv[32];
 DXGKARG_RENDER r={2,2,list,patches,dma,priv};struct geo_copy_packet p={GEO_COPY_MAGIC,sizeof(p),1,0,1,800,600,0};
 if(mode==8)p.reserved=1;dst.refs=src.refs=0;writes=syncs=0;
 NTSTATUS result=GeoRenderCopyImpl(&c,&r,&p);
 if(mode==0){assert(result==0&&!a.engine.poisoned&&session.state==1&&dst.refs==1&&src.refs==1);assert(r.pPatchLocationListOut==patches+2);}
 else if(mode==1||mode==8){assert(result<0&&!a.engine.poisoned&&session.state==1&&dst.refs==0&&src.refs==0);}
 else{assert(result<0&&session.state==GEO_SESSION_FAILED);if(mode!=7)assert(a.engine.poisoned);if(mode!=5)assert(dst.refs==1&&src.refs==1);assert(r.pDmaBuffer==dma&&r.pDmaBufferPrivateData==priv);}
 /* No tested failure path drops either allocation reference. Failed state
  * is consumed by existing GeoSessionEnter Render/Submit admission gate. */
 cases++;
 }printf("actual Copy Render gate338: %u cases passed; refs retained, no fake stop; mocked owner/ticket failures\n",cases);return 0;}
'''
f=out/'actual-copy-gate.c';f.write_text(code);cmd=['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(f),'-o',str(out/'test')]
x=subprocess.run(cmd,capture_output=True,text=True);(out/'compile.log').write_text(x.stdout+x.stderr);x.check_returncode()
x=subprocess.run([str(out/'test')],capture_output=True,text=True);(out/'test.log').write_text(x.stdout+x.stderr);print(x.stdout+x.stderr,end='');x.check_returncode()
(out/'receipt.json').write_text(json.dumps({'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'cases_passed':9,'actual_functions_extracted':True,'ASAN_UBSAN':True,'real_hardware':False,'fake_stop_or_ref_release_added':False},indent=2)+'\n')
