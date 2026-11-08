#!/usr/bin/env python3
"""Execute actual GeoPatchImpl/inspector with minimal WDK and native callback mocks."""
import argparse,subprocess,json,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[2];o=a.output.resolve();o.mkdir(parents=True,exist_ok=False)
s=(r/'drivers/gpu/mt8167/wddm-render/kmd_clear.c').read_text();actual=s[s.index('struct inspect_ticket {'):s.index('\nstruct submit_call ')]
pre=r'''#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include "clear_engine.h"
#include "frontend/draw_abi.h"
typedef unsigned char UCHAR;
typedef uint32_t UINT,ULONG;typedef int32_t NTSTATUS;typedef void *HANDLE;typedef int BOOLEAN;
#define TRUE 1
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER ((int32_t)0xc000000d)
#define STATUS_DEVICE_NOT_READY ((int32_t)0xc00000a3)
#define STATUS_NOT_SUPPORTED ((int32_t)0xc00000bb)
#define RtlZeroMemory(p,n) memset(p,0,n)
struct geo_kmd_adapter{struct geo_engine engine;BOOLEAN (*synchronize)(void*,BOOLEAN(*)(void*),void*);void *sync_context;};
typedef struct{UINT AllocationIndex,Value,DriverId,AllocationOffset,PatchOffset,SplitOffset;}D3DDDI_PATCHLOCATIONLIST;
typedef struct{HANDLE hDeviceSpecificAllocation;UINT WriteOperation;}DXGK_ALLOCATIONLIST;
typedef struct{HANDLE hContext;UINT EngineOrdinal,DmaBufferSegmentId;struct{UINT Value;}Flags;UINT DmaBufferSize,DmaBufferSubmissionStartOffset,DmaBufferSubmissionEndOffset,DmaBufferPrivateDataSize,DmaBufferPrivateDataSubmissionStartOffset,DmaBufferPrivateDataSubmissionEndOffset;void*pDmaBuffer,*pDmaBufferPrivateData;const DXGK_ALLOCATIONLIST*pAllocationList;UINT AllocationListSize;const D3DDDI_PATCHLOCATIONLIST*pPatchLocationList;UINT PatchLocationListSize,PatchLocationListSubmissionStart,PatchLocationListSubmissionLength;}DXGKARG_PATCH;
static UINT phase,lastkind,checkpoints,prepare_calls,owner_calls;static NTSTATUS laststatus,prepare_status;static int sync_ok=1;
static void GeoFactoryCheckpoint(UINT stage,NTSTATUS status,UINT a,UINT b,UINT c,UINT d){(void)c;(void)d;if(stage==18){phase=a;lastkind=b;laststatus=status;checkpoints++;}}
static int ticket_range(UINT size,UINT begin,UINT end){return begin<=size&&end>=begin&&end-begin==16&&end<=size;}
static NTSTATUS GeoReadTicket(const void*p,struct geo_dma_ticket*t){memcpy(t,p,sizeof(*t));return 0;}
static BOOLEAN synchronize(void*c,BOOLEAN(*f)(void*),void*p){(void)c;return sync_ok?f(p):0;}
static NTSTATUS prepare_pending(struct geo_kmd_adapter*a,const struct geo_dma_ticket*t){prepare_calls++;struct geo_job*j=&a->engine.jobs[t->slot];if(j->state==1)return 0;owner_calls++;if(prepare_status)return prepare_status;j->state=1;j->owner_job=99;return 0;}
'''
post=r'''
int main(void){struct geo_kmd_adapter a={0};struct geo_allocation dst={.surface=1,.bound=1},src={.surface=2,.bound=1};struct geo_dma_ticket t={GEO_TICKET_MAGIC,0,7,0},priv=t;struct geo_job*j=&a.engine.jobs[0];HANDLE ctx=(void*)3,dh=(void*)4,sh=(void*)5;
 a.synchronize=synchronize;*j=(struct geo_job){.allocation=&dst,.vertex=&src,.context=ctx,.open_handle=dh,.vertex_open_handle=sh,.generation=7,.state=4,.kind=GEO_COPY_MAGIC};
 DXGK_ALLOCATIONLIST list[3]={{dh,1},{sh,0},{0,0}};D3DDDI_PATCHLOCATIONLIST patches[3]={{.AllocationIndex=0,.DriverId=GEO_COPY_MAGIC},{.AllocationIndex=1,.DriverId=GEO_COPY_MAGIC}};
 DXGKARG_PATCH p={.hContext=ctx,.DmaBufferSize=16,.DmaBufferSubmissionEndOffset=16,.DmaBufferPrivateDataSize=16,.DmaBufferPrivateDataSubmissionEndOffset=16,.pDmaBuffer=&t,.pDmaBufferPrivateData=&priv,.pAllocationList=list,.AllocationListSize=3,.pPatchLocationList=patches,.PatchLocationListSize=3,.PatchLocationListSubmissionLength=2};
 p.hContext=(void*)6;assert(GeoPatchImpl(&a,&p)==STATUS_INVALID_PARAMETER&&phase==5&&!owner_calls);p.hContext=ctx;
 list[1].hDeviceSpecificAllocation=(void*)6;assert(GeoPatchImpl(&a,&p)==STATUS_INVALID_PARAMETER&&phase==5&&!owner_calls);list[1].hDeviceSpecificAllocation=sh;
 src.bound=0;assert(GeoPatchImpl(&a,&p)==STATUS_INVALID_PARAMETER&&phase==5&&!owner_calls);src.bound=1;
 list[1].WriteOperation=1;assert(GeoPatchImpl(&a,&p)==STATUS_INVALID_PARAMETER&&phase==3&&!owner_calls);list[1].WriteOperation=0;
 priv.generation=8;assert(GeoPatchImpl(&a,&p)==STATUS_INVALID_PARAMETER&&phase==4&&!owner_calls);priv=t;
 prepare_status=STATUS_NOT_SUPPORTED;assert(GeoPatchImpl(&a,&p)==prepare_status&&phase==6&&laststatus==prepare_status&&lastkind==GEO_COPY_MAGIC&&owner_calls==1&&j->state==4);prepare_status=0;
 assert(!GeoPatchImpl(&a,&p)&&owner_calls==2&&j->state==1&&j->owner_job==99);unsigned checkpoints_before=checkpoints;
 assert(!GeoPatchImpl(&a,&p)&&owner_calls==2&&j->owner_job==99&&checkpoints==checkpoints_before);
 j->state=2;assert(GeoPatchImpl(&a,&p)==STATUS_INVALID_PARAMETER&&phase==5&&owner_calls==2);
 sync_ok=0;assert(GeoPatchImpl(&a,&p)==STATUS_DEVICE_NOT_READY&&phase==5&&laststatus==STATUS_DEVICE_NOT_READY);
 puts("actual GeoPatch: validate before native / 2refs / same prepared-token repatch / exact error phase PASS");return 0;}
'''
f=o/'actual-patch.c';f.write_text(pre+actual+post)
x=subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g','-I'+str(r/'drivers/gpu/mt8167/wddm-render'),str(f),'-o',str(o/'test')],capture_output=True,text=True);(o/'compile.log').write_text(x.stdout+x.stderr);x.check_returncode()
x=subprocess.run([str(o/'test')],capture_output=True,text=True);(o/'test.log').write_text(x.stdout+x.stderr);print(x.stdout+x.stderr,end='');x.check_returncode()
(o/'verification.json').write_text(json.dumps({'actual_source_sha256':hashlib.sha256(actual.encode()).hexdigest(),'native_and_WDK_mocked':True,'ASAN_UBSAN':'PASS','hardware_verified':False},indent=2)+'\n')
