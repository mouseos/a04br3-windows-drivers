#!/usr/bin/env python3
"""Actual Patch and resident-preparation functions with WDK/owner mocks."""
import argparse,subprocess,json,hashlib,re
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parents[2];out=a.output.resolve()
if not out.is_relative_to(r) or out.exists():raise ValueError('New checkout-local output required')
out.mkdir(parents=True);source=r/'drivers/gpu/mt8167/wddm-render/kmd_clear.c';s=source.read_text()
def function(name):
 for m in re.finditer(r'^static[^\n]*\b'+name+r'\(',s,re.M):
  brace=s.index('{',m.start());semi=s.find(';',m.start())
  if 0<=semi<brace:continue
  i=brace+1;depth=1
  while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
  return s[m.start():i]
 raise ValueError(name)

inspect=s[s.index('struct inspect_ticket {'):s.index('static BOOLEAN inspect_prepared(')]
actual='\n'.join([function('ticket_range'),inspect,function('inspect_prepared'),function('prepare_resident_dependents'),function('GeoPatchImpl')])
pre=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef int32_t NTSTATUS;typedef unsigned UINT;typedef int BOOLEAN;typedef void*HANDLE;typedef unsigned char UCHAR;
#define TRUE 1
#define PASSIVE_LEVEL 0
#define STATUS_SUCCESS 0
#define STATUS_INVALID_PARAMETER -1
#define STATUS_NOT_SUPPORTED -2
#define STATUS_DEVICE_NOT_READY -3
#define GEO_MAX_JOBS 2
#define GEO_TICKET_MAGIC 1
#define GEO_CLEAR_MAGIC 11
#define GEO_DRAW_MAGIC 12
#define GEO_COPY_MAGIC 13
#define RtlZeroMemory(p,n) memset(p,0,n)
struct geo_dma_ticket{UINT magic,slot,generation,reserved;};
struct geo_allocation{int bound;};
struct geo_job{UINT state,generation,kind;struct geo_allocation*allocation,*vertex,*texture;HANDLE context,open_handle,vertex_open_handle,texture_open_handle;};
struct geo_kmd_adapter{struct{int poisoned;struct geo_job jobs[2];}engine;BOOLEAN(*synchronize)(void*,BOOLEAN(*)(void*),void*);void*sync_context;};
typedef struct{UINT AllocationIndex,Value,DriverId,AllocationOffset,PatchOffset,SplitOffset;}D3DDDI_PATCHLOCATIONLIST;
struct alist{HANDLE hDeviceSpecificAllocation;UINT WriteOperation;};
typedef struct{HANDLE hContext;UINT EngineOrdinal,DmaBufferSegmentId;struct{UINT Value;}Flags;D3DDDI_PATCHLOCATIONLIST*pPatchLocationList;struct alist*pAllocationList;void*pDmaBuffer,*pDmaBufferPrivateData;UINT PatchLocationListSubmissionStart,PatchLocationListSize,PatchLocationListSubmissionLength,DmaBufferSize,DmaBufferSubmissionStartOffset,DmaBufferSubmissionEndOffset,DmaBufferPrivateDataSize,DmaBufferPrivateDataSubmissionStartOffset,DmaBufferPrivateDataSubmissionEndOffset,AllocationListSize;}DXGKARG_PATCH;
static unsigned prepares;static NTSTATUS injected;
static UINT KeGetCurrentIrql(void){return 0;}
static NTSTATUS prepare_pending(struct geo_kmd_adapter*a,const struct geo_dma_ticket*t){prepares++;if(injected)return injected;a->engine.jobs[t->slot].state=1;return 0;}
static NTSTATUS GeoReadTicket(void*p,struct geo_dma_ticket*t){memcpy(t,p,sizeof(*t));return 0;}
#define GeoFactoryCheckpoint(...) ((void)0)
static BOOLEAN sync_mock(void*p,BOOLEAN(*f)(void*),void*x){(void)p;return f(x);}
'''
post=r'''
int main(void){unsigned cases=0;
 for(unsigned order=0;order<2;order++){
  struct geo_allocation dst={0},src={0};struct geo_kmd_adapter a={0};a.synchronize=sync_mock;
  struct geo_job*j=&a.engine.jobs[0];*j=(struct geo_job){4,7,GEO_COPY_MAGIC,&dst,&src,NULL,(void*)1,(void*)2,(void*)3,NULL};
  struct geo_dma_ticket t={GEO_TICKET_MAGIC,0,7,0};D3DDDI_PATCHLOCATIONLIST patch[2]={{0,0,GEO_COPY_MAGIC,0,0,0},{1,0,GEO_COPY_MAGIC,0,0,0}};struct alist allocations[2]={{(void*)2,1},{(void*)3,0}};
  DXGKARG_PATCH p={0};p.hContext=(void*)1;p.pPatchLocationList=patch;p.pAllocationList=allocations;p.pDmaBuffer=p.pDmaBufferPrivateData=&t;p.PatchLocationListSize=p.PatchLocationListSubmissionLength=p.AllocationListSize=2;p.DmaBufferSize=p.DmaBufferSubmissionEndOffset=p.DmaBufferPrivateDataSize=p.DmaBufferPrivateDataSubmissionEndOffset=sizeof(t);
  prepares=0;struct geo_allocation*first=order?&src:&dst,*last=order?&dst:&src;first->bound=1;
  assert(!prepare_resident_dependents(&a,first)&&prepares==0&&j->state==4);cases++;
  /* Unprepared is still rejected: never a catch-all Patch success. */
  assert(GeoPatchImpl(&a,&p)<0&&prepares==0);cases++;
  last->bound=1;assert(!prepare_resident_dependents(&a,last)&&prepares==1&&j->state==1);cases++;
  assert(!prepare_resident_dependents(&a,first)&&prepares==1);cases++;
  for(int i=0;i<8;i++)assert(!GeoPatchImpl(&a,&p)&&prepares==1);cases+=8;
  t.generation++;assert(GeoPatchImpl(&a,&p)<0);t.generation--;cases++;
  allocations[1].hDeviceSpecificAllocation=(void*)4;assert(GeoPatchImpl(&a,&p)<0);allocations[1].hDeviceSpecificAllocation=(void*)3;cases++;
  p.hContext=(void*)4;assert(GeoPatchImpl(&a,&p)<0);p.hContext=(void*)1;cases++;
  patch[1].DriverId=GEO_DRAW_MAGIC;assert(GeoPatchImpl(&a,&p)<0);patch[1].DriverId=GEO_COPY_MAGIC;cases++;
  a.engine.poisoned=1;assert(GeoPatchImpl(&a,&p)<0);a.engine.poisoned=0;cases++;
  j->state=4;injected=-42;assert(prepare_resident_dependents(&a,last)==-42&&j->state==4);injected=0;cases++;
 }
 printf("actual Patch341: %u cases; both MAP orders, idempotent repatch, strict context/ticket/handle gates; owner/WDK mocked\n",cases);return 0;}
'''
f=out/'actual-patch.c';f.write_text(pre+actual+post)
x=subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(f),'-o',str(out/'test')],capture_output=True,text=True);(out/'compile.log').write_text(x.stdout+x.stderr);x.check_returncode()
x=subprocess.run([str(out/'test')],capture_output=True,text=True);(out/'test.log').write_text(x.stdout+x.stderr);print(x.stdout+x.stderr,end='');x.check_returncode()
(out/'receipt.json').write_text(json.dumps({'actual_KMD_source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'ASAN_UBSAN':True,'owner_WDK_mocked':True,'hardware_verified':False,'native_work_in_Patch':False},indent=2)+'\n')
